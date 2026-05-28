#!/usr/bin/env python3
"""
SalakhovaClient.py — консольный Python-клиент для протокола Салаховой (pull-модель).
Только стандартная библиотека (socket, struct, threading).
"""

import socket
import struct
import threading
import sys
import time

# ── Константы протокола ──────────────────────────────────────────────
MT_CLOSE   = 0
MT_DATA    = 1
MT_START   = 2
MT_STOP    = 3
MT_QUIT    = 4
MT_INFO    = 5
MT_CONFIRM = 6
MT_GETDATA = 7
MT_NODATA  = 8

ADDR_BROADCAST = -1
ADDR_SERVER    = -2

PORT = 12345
HEADER_FMT = '<iiii'
HEADER_SIZE = 16

POLL_INTERVAL = 1.0  # секунд между MT_GETDATA


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


class SalakhovaClient:
    """Клиент протокола Салаховой — pull-модель с poll-потоком."""

    def __init__(self):
        self.sock = None
        self.client_id = None
        self.connected = False
        self.send_lock = threading.Lock()
        self.print_lock = threading.Lock()
        self.clients = {}

    def _safe_print(self, text: str):
        """Потокобезопасный вывод (poll-поток и главный поток конкурируют)."""
        with self.print_lock:
            print(text)

    def connect(self, host: str, port: int = PORT):
        """Подключиться к серверу, выполнить handshake (MT_CONFIRM → client_id)."""
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.connect((host, port))
        self.connected = True

        # Handshake: первое сообщение — MT_CONFIRM с ID клиента
        msg_type, _, _, _, payload = self._recv_message()
        if msg_type != MT_CONFIRM:
            raise RuntimeError(
                f"Ожидался MT_CONFIRM при handshake, получен MT_{msg_type}"
            )
        self.client_id = int(payload)

    def disconnect(self):
        """Отправить MT_QUIT серверу и закрыть сокет."""
        if self.connected and self.sock:
            self.connected = False
            try:
                self._send(ADDR_SERVER, MT_QUIT)
            except OSError:
                pass
            try:
                self.sock.close()
            except OSError:
                pass

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

    def _send_header(self, msg_type: int, size: int, to_addr: int, from_addr: int):
        """Отправить только заголовок без payload."""
        header = struct.pack(HEADER_FMT, msg_type, size, to_addr, from_addr)
        with self.send_lock:
            self.sock.sendall(header)

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

    def poll_loop(self):
        """Фоновый poll-поток: шлёт MT_GETDATA, принимает MT_DATA/MT_NODATA/MT_CONFIRM."""
        while self.connected:
            try:
                # Запрашиваем данные
                self._send_header(MT_GETDATA, 0, ADDR_SERVER, self.client_id)

                # Читаем ответ
                msg_type, from_addr, to_addr, payload = self._recv_message()
                if msg_type is None:
                    self.connected = False
                    break

                if msg_type == MT_DATA:
                    sender = from_addr
                    name = self.clients.get(sender, f"#{sender}")
                    self._safe_print(f"\n[From Client #{sender} ({name})]: {payload}")

                elif msg_type == MT_CONFIRM:
                    if payload and (';' in payload or ':' in payload):
                        self.clients.update(parse_clients_payload(payload))
                        self._safe_print(f"\n[Clients updated: {len(self.clients)} online]")

                elif msg_type == MT_NODATA:
                    pass  # Данных нет, просто ждём

                elif msg_type == MT_CLOSE:
                    self._safe_print(f"\n⚠ Соединение закрыто сервером: {payload}")
                    self.connected = False
                    break

                time.sleep(POLL_INTERVAL)

            except (OSError, ConnectionError, struct.error) as exc:
                if self.connected:
                    self._safe_print(f"\n⚠ Соединение потеряно: {exc}")
                self.connected = False
                break


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

    # Запускаем poll-поток
    poll_thread = threading.Thread(target=client.poll_loop, daemon=True)
    poll_thread.start()

    # Основной цикл ввода команд
    try:
        while client.connected:
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