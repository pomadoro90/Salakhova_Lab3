#!/usr/bin/env python3
"""
SalakhovaClient.py — консольный Python-клиент для протокола Салаховой.
Только стандартная библиотека (socket, struct, threading, collections).
"""

import socket
import struct
import threading
from collections import deque
from datetime import datetime
import sys

# ── Константы протокола ──────────────────────────────────────────────
MT_CLOSE   = 0
MT_DATA    = 1
MT_START   = 2
MT_STOP    = 3
MT_QUIT    = 4
MT_INFO    = 5
MT_CONFIRM = 6

ADDR_BROADCAST = -1
ADDR_SERVER    = -2

PORT = 12345
HEADER_FMT = '<iiii'
HEADER_SIZE = 16

PING_INTERVAL = 10.0  # секунд между MT_INFO
POLL_INTERVAL = 0.1   # секунд между опросами inbox в printer-потоке


class SalakhovaClient:
    """Клиент протокола Салаховой — push-модель, только TCP."""

    def __init__(self):
        self.sock = None
        self.client_id = None       # int ID, полученный от сервера
        self.alive = False
        self.inbox = deque()
        self.inbox_lock = threading.Lock()
        self.send_lock = threading.Lock()
        self.clients = {}           # id -> name (строка)

    # ── Публичный API ────────────────────────────────────────────────

    def connect(self, host: str, port: int = PORT):
        """Подключиться к серверу, выполнить handshake (MT_CONFIRM → client_id)."""
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.connect((host, port))
        self.alive = True

        # Handshake: первое сообщение — MT_CONFIRM с ID клиента
        msg_type, _, _, _, payload = self._recv_message()
        if msg_type != MT_CONFIRM:
            raise RuntimeError(
                f"Ожидался MT_CONFIRM при handshake, получен MT_{msg_type}"
            )
        self.client_id = int(payload)

        # Запускаем reader-поток
        reader = threading.Thread(target=self._reader_loop, daemon=True)
        reader.start()

    def poll(self):
        """Извлечь одно сообщение из inbox.
        Возвращает (source, cmd, target, text) или None.
        """
        with self.inbox_lock:
            if not self.inbox:
                return None
            return self.inbox.popleft()

    def disconnect(self):
        """Отправить MT_QUIT серверу и закрыть сокет."""
        if self.alive and self.sock:
            self.alive = False
            try:
                self._send(ADDR_SERVER, MT_QUIT)
            except OSError:
                pass
            try:
                self.sock.close()
            except OSError:
                pass

    # ── Внутренние методы ────────────────────────────────────────────

    def _send(self, target: int, msg_type: int, text: str = ""):
        """Упаковать и отправить сообщение."""
        payload = text.encode('utf-16-le')
        header = struct.pack(
            HEADER_FMT,
            msg_type,
            len(payload),
            target,
            self.client_id or 0,
        )
        with self.send_lock:
            self.sock.sendall(header + payload)

    def _reader_loop(self):
        """Фоновый поток: читать заголовки + payload, класть в inbox."""
        while self.alive and self.sock:
            try:
                msg_type, source, target, payload = self._recv_message()
                if msg_type is None:
                    break  # соединение закрыто
            except (OSError, ConnectionError, struct.error) as exc:
                if self.alive:
                    with self.inbox_lock:
                        self.inbox.append(
                            (0, MT_CLOSE, 0, f"Соединение потеряно: {exc}")
                        )
                break

            with self.inbox_lock:
                self.inbox.append((source, msg_type, target, payload))

    def _recv_message(self):
        """Прочитать одно сообщение: 16 байт заголовок + payload.
        Возвращает кортеж (msg_type, source, target, payload_text).
        При закрытии соединения возвращает (None, None, None, None).
        """
        raw_header = self._recv_exact(HEADER_SIZE)
        if not raw_header:
            return None, None, None, None

        msg_type, size, to_addr, from_addr = struct.unpack(
            HEADER_FMT, raw_header
        )

        payload = ""
        if size > 0:
            raw_payload = self._recv_exact(size)
            if raw_payload is None:
                return None, None, None, None
            payload = raw_payload.decode('utf-16-le')

        return msg_type, from_addr, to_addr, payload

    def _recv_exact(self, n: int):
        """Прочитать ровно n байт из сокета.
        Возвращает bytes или None при закрытии соединения.
        """
        chunks = []
        remaining = n
        while remaining > 0:
            try:
                chunk = self.sock.recv(remaining)
            except OSError:
                return None
            if not chunk:
                return None
            chunks.append(chunk)
            remaining -= len(chunk)
        return b''.join(chunks)


# ── Вспомогательные функции main() ──────────────────────────────────

def parse_clients_payload(payload: str) -> dict:
    """Парсит payload MT_CONFIRM вида '1:Alice;2:Bob;3:Charlie'.
    Возвращает словарь {id: name}.
    """
    result = {}
    parts = payload.split(';')
    for part in parts:
        part = part.strip()
        if not part:
            continue
        if ':' in part:
            cid, _, name = part.partition(':')
            try:
                result[int(cid)] = name
            except ValueError:
                pass
    return result


def format_timestamp() -> str:
    return datetime.now().strftime('%H:%M:%S')


def printer_loop(client: SalakhovaClient):
    """Фоновый поток: читает из inbox и выводит сообщения."""
    while client.alive:
        msg = client.poll()
        if msg is None:
            threading.Event().wait(POLL_INTERVAL)
            continue

        source, cmd, target, text = msg

        if cmd == MT_DATA:
            ts = format_timestamp()
            if source == ADDR_SERVER:
                print(f"[{ts}] <сервер>: {text}")
            else:
                name = client.clients.get(source, f"#{source}")
                print(f"[{ts}] {name}: {text}")

        elif cmd == MT_CONFIRM:
            if text and (';' in text or ':' in text):
                client.clients.update(parse_clients_payload(text))
            # Если это просто ID (handshake) — игнорируем, ID уже сохранён

        elif cmd == MT_CLOSE:
            print(f"⚠ {text}")
            client.alive = False
            break


def ping_loop(client: SalakhovaClient):
    """Фоновый поток: каждые 10 сек отправляет MT_INFO серверу."""
    while client.alive:
        try:
            client._send(ADDR_SERVER, MT_INFO)
        except OSError:
            break
        threading.Event().wait(PING_INTERVAL)


def main():
    host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
    port = int(sys.argv[2]) if len(sys.argv) > 2 else PORT

    client = SalakhovaClient()
    try:
        client.connect(host, port)
    except (ConnectionRefusedError, socket.gaierror, OSError) as exc:
        print(f"Ошибка подключения к {host}:{port} — {exc}")
        sys.exit(1)
    except RuntimeError as exc:
        print(f"Ошибка handshake: {exc}")
        sys.exit(1)

    print(f"Подключено как #{client.client_id}")
    print("Команды: /list, /send <id|all> <текст>, /quit")

    # Поток пинга
    ping_thread = threading.Thread(target=ping_loop, args=(client,), daemon=True)
    ping_thread.start()

    # Поток вывода
    printer_thread = threading.Thread(
        target=printer_loop, args=(client,), daemon=True
    )
    printer_thread.start()

    # Основной цикл ввода команд
    try:
        while client.alive:
            try:
                line = input().strip()
            except (EOFError, KeyboardInterrupt):
                print()
                break

            if not line:
                continue

            if line == '/quit':
                break

            elif line == '/list':
                if client.clients:
                    for cid, name in sorted(client.clients.items()):
                        marker = " ← это вы" if cid == client.client_id else ""
                        print(f"  #{cid}: {name}{marker}")
                else:
                    print("  Список клиентов пуст")

            elif line.startswith('/send '):
                rest = line[6:].strip()
                if not rest:
                    print("Использование: /send <id|all> <текст>")
                    continue

                # Первое слово — id или all, остальное — текст
                parts = rest.split(maxsplit=1)
                target_str = parts[0]
                text = parts[1] if len(parts) > 1 else ""

                if target_str == 'all':
                    target = ADDR_BROADCAST
                else:
                    try:
                        target = int(target_str)
                    except ValueError:
                        print(f"Неверный ID: {target_str}")
                        continue

                try:
                    client._send(target, MT_DATA, text)
                except OSError as exc:
                    print(f"Ошибка отправки: {exc}")
                    break

            else:
                print("Неизвестная команда. Доступно: /list, /send, /quit")

    finally:
        client.disconnect()
        print("Отключено.")


if __name__ == '__main__':
    main()
