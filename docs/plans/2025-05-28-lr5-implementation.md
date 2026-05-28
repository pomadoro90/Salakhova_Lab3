# ЛР5: Реализация плана — Диалоговый (C#) + Консольный (Python) клиенты без DLL

> **For Hermes:** Use subagent-driven-development skill to implement this plan task-by-task.

**Goal:** Создать два клиентских приложения для сервера сообщений из ЛР4: диалоговый WinForms на C# (без Salakhova_Transport.dll, через System.Net.Sockets.Socket) и консольный на Python (через socket), адаптировав протокол Салаховой.

**Architecture:** Сервер из ЛР4 (порт 12345, boost::asio, push-модель, MT_START/MT_DATA/MT_CONFIRM/MT_QUIT/MT_INFO/MT_CLOSE). Оба клиента работают через прямые TCP-сокеты, используют протокол Салаховой: заголовок 16 байт `[messageType(4)][size(4)][to(4)][from(4)]`, данные в UTF-16LE. Сервер push'ит сообщения через SessionSender-поток — клиент просто читает incoming.

**Tech Stack:** C# .NET 8 / WinForms, Python 3 (stdlib socket + struct), протокол Салаховой.

---

## Протокол Салаховой (ключевые детали)

**Заголовок (16 байт, little-endian int32):**
```
messageType = int32  |  см. enum MessageTypes
size        = int32  |  длина payload в байтах (data.length * sizeof(wchar_t))
to          = int32  |  ADDR_BROADCAST=-1, ADDR_SERVER=-2, или ID клиента
from        = int32  |  заполняется сервером (0 при отправке от клиента — сервер подставит)
```

**Payload:** UTF-16LE (wstring в C++), размер = size байт.

**Константы:**
- MT_CLOSE=0, MT_DATA=1, MT_START=2, MT_STOP=3, MT_QUIT=4, MT_INFO=5, MT_CONFIRM=6
- ADDR_BROADCAST=-1, ADDR_SERVER=-2

**Handshake (при подключении):**
1. Клиент подключается по TCP
2. Сервер авто-присваивает ID (nextClientId++) и отправляет MT_CONFIRM с payload = to_wstring(clientId) (например "1", "2"...)
3. Сервер рассылает всем MT_CONFIRM с payload = "id:Name;id:Name;..."
4. После этого сервер читает сообщения от клиента в цикле

**Регулярная работа:**
- Клиент шлёт MT_DATA (to=ADDR_BROADCAST или id клиента) для отправки сообщения
- Клиент шлёт MT_INFO (to=ADDR_SERVER) для пинга (сервер отвечает списком клиентов MT_CONFIRM)
- Клиент шлёт MT_QUIT (to=ADDR_SERVER) для отключения
- Сервер шлёт MT_CONFIRM (payload = "id:Name;...") при изменении списка или по пингу
- Сервер шлёт MT_DATA (from=sender_id) для входящих сообщений
- Сервер шлёт MT_CLOSE при отключении клиента по таймауту

**Важное отличие от Вельгана:** Вельган использует polling (VM_GETDATA). Салахова использует push — сервер автоматически отправляет данные через SessionSender-поток. Клиент НЕ должен слать GETDATA — он просто слушает сокет в фоновом потоке.

---

## Задачи

### Task 1: Создать SalakhovaSocketClient.cs — класс C# Socket-клиента

**Objective:** Написать класс, инкапсулирующий работу с TCP-сокетом по протоколу Салаховой (без DLL).

**Files:**
- Create: `lab1_sysprog/SalakhovaSocketClient.cs`

**Класс SalakhovaSocketClient:**
- Public API: `Connect(host, port)`, `Disconnect()`, `Send(target, messageType, text)`, `Poll(out cmd, out source, out target, out text)`, `IsConnected`, `ClientId`
- Константы протокола: MT_CLOSE=0, MT_DATA=1, MT_START=2, MT_STOP=3, MT_QUIT=4, MT_INFO=5, MT_CONFIRM=6, ADDR_BROADCAST=-1, ADDR_SERVER=-2
- HeaderSize = 16, формат: `[messageType(4)][size(4)][to(4)][from(4)]` (little-endian int32)
- Connect: создать Socket, подключиться, прочитать первый MT_CONFIRM — вытащить clientId из payload (текстовое представление числа), запустить ReaderLoop-поток
- Disconnect: отправить MT_QUIT (to=ADDR_SERVER), закрыть сокет
- Send: упаковать header + payload (UTF-16LE), отправить
- ReaderLoop: фоновый поток, читает 16-байтный header, потом payload, складывает в ConcurrentQueue
- Poll: достаёт из очереди, возвращает true/false
- PackHeader: статический, формат как у сервера — messageType, size, to, from (отличается от Вельгана!)
- PackPayload: Encoding.Unicode.GetBytes (UTF-16LE)
- ReadExact: читает ровно N байт из Socket
- UnpackHeader: парсит 16 байт → (messageType, size, to, from)

**Ключевое отличие от Вельгана:** Вельган шлёт `[targetId, sourceId, commandType, dataSize]`, а Салахова — `[messageType, size, to, from]`. Это РАЗНЫЙ порядок полей! Обязательно использовать порядок Салаховой.

### Task 2: Переписать Form1.cs — диалоговый клиент без DLL

**Objective:** Переписать Form1.cs, убрав все DllImport и используя SalakhovaSocketClient.

**Files:**
- Modify: `lab1_sysprog/Form1.cs`
- Modify: `lab1_sysprog/Form1.Designer.cs` (добавить txtHost, numericPort)

**Изменения в Form1.cs:**
- Удалить все DllImport (Salakhova_Connect, Salakhova_Disconnect, Salakhova_IsConnected, Salakhova_Send, Salakhova_Poll)
- Заменить все вызовы DLL на методы SalakhovaSocketClient
- Connect: `client.Connect(host, port)` вместо `Salakhova_Connect(...)`
- Disconnect: `client.Disconnect()` вместо `Salakhova_Disconnect()`
- Пинг-таймер (каждые 10 сек): `client.Send(ADDR_SERVER, MT_INFO, "")` вместо `Salakhova_Send(ADDR_SERVER, MT_INFO, "")`
- Отправка: `client.Send(targetId, MT_DATA, text)` вместо `Salakhova_Send(targetId, MT_DATA, text)`
- PollTimer_Tick: `client.Poll(out cmd, out source, out target, out text)` вместо `Salakhova_Poll(...)`
- Обработка: если cmd == MT_CONFIRM и payload不含 ":" — это ID; если содержит ":" — это список клиентов
- Добавить txtHost и numericPort в Designer (как у Вельгана) — для ввода хоста и порта
- Убрать StringBuilder — Poll теперь возвращает string
- FormClosing: Disconnect

**Изменения в Form1.Designer.cs:**
- Добавить txtHost (TextBox) и numericPort (NumericUpDown) — по аналогии с Вельганом
- btnConnect/btnDisconnect остаются
- txtOutput, comboRecipient, textBoxMessage, btnSend остаются

### Task 3: Обновить lab1_sysprog.csproj — убрать DLL-зависимости

**Objective:** Убедиться, что csproj не содержит ссылок на DLL или COM-компонентов.

**Files:**
- Verify: `lab1_sysprog/lab1_sysprog.csproj`

Текущий csproj уже чист — только net8.0-windows и UseWindowsForms. Проверить, что нет нативных ссылок.

### Task 4: Создать SalakhovaClient.py — консольный Python-клиент

**Objective:** Написать Python-клиент (без внешних библиотек, только socket/struct/threading) по протоколу Салаховой.

**Files:**
- Create: `lab1_console/SalakhovaClient.py` (рядом с сервером, логично)

**Структура клиента:**

```python
# Константы протокола Салаховой
MT_CLOSE = 0; MT_DATA = 1; MT_START = 2; MT_STOP = 3
MT_QUIT = 4; MT_INFO = 5; MT_CONFIRM = 6
ADDR_BROADCAST = -1; ADDR_SERVER = -2
HEADER_FMT = '<iiii'  # messageType, size, to, from
HEADER_SIZE = 16
PORT = 12345
```

**Класс SalakhovaClient:**
- `__init__`: sock=None, client_id=None, alive=False, inbox=deque(), inbox_lock, send_lock
- `connect(host, port)`: TCP connect, прочитать первый MT_CONFIRM → извлечь client_id из payload, запустить _reader_loop в daemon-потоке
- `_send(target, msg_type, text="")`: pack header `[msg_type, len_payload, target, client_id]` + payload (utf-16-le), sendall
- `_reader_loop()`: читать 16-байт header, потом payload, класть в inbox. Если исключение — alive=False, break
- `_recv_exact(n)`: читать ровно n байт
- `poll()`: вернуть (source, cmd, target, text) из inbox или None
- `disconnect()`: отправить MT_QUIT (to=ADDR_SERVER), закрыть сокет

**main():**
- Аргументы: host (default 127.0.0.1), port (default 12345)
- Подключение: `client.connect(host, port)`
- Пинг-поток: каждые 10 сек шлёт MT_INFO (to=ADDR_SERVER)
- Printer-поток: опрашивает poll(), выводит MT_DATA в консоль, MT_CONFIRM — обновляет список клиентов
- Основной поток: ввод команд `/list`, `/send <id|all> <text>`, `/quit`
- При `/list` — показать известных клиентов
- При `/send` — отправить MT_DATA указанному адресату или broadcast

### Task 5: Протестировать сборку C# проекта

**Objective:** Убедиться, что проект компилируется без ошибок.

**Commands:**
```bash
cd /home/pomadoro/github/Salakhova_Lab3/lab1_sysprog
dotnet build
```

Если ошибки — исправить и пересобрать.

### Task 6: Закоммитить и запушить

**Objective:** Сохранить все изменения в git.

**Commands:**
```bash
cd /home/pomadoro/github/Salakhova_Lab3
git add lab1_sysprog/SalakhovaSocketClient.cs lab1_sysprog/Form1.cs lab1_sysprog/Form1.Designer.cs lab1_console/SalakhovaClient.py
git commit -m "feat: Lab 5 - C# Socket client (no DLL) + Python console client"
git push origin main
```

---

## Порядок выполнения

1. Task 1 (SalakhovaSocketClient.cs) — независимый, можно делегировать сразу
2. Task 2 (Form1.cs) — зависит от Task 1 (нужен SalakhovaSocketClient API)
3. Task 3 (csproj) — быстрый, независимый
4. Task 4 (Python) — независимый, можно параллельно с Task 1
5. Task 5 (build) — после Tasks 1-3
6. Task 6 (git) — после всех