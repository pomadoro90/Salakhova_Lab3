using System;
using System.Net;
using System.Net.Sockets;
using System.Text;

namespace Salakhova_Sharp
{
    /// <summary>
    /// Socket-клиент для протокола Салаховой.
    /// Замена Salakhova_Transport.dll — работает напрямую через System.Net.Sockets.Socket.
    /// 
    /// Формат заголовка (16 байт, little-endian int32):
    ///   [0..3]  - messageType (int)
    ///   [4..7]  - size (int)         — длина payload в байтах
    ///   [8..11] - to (int)           — ADDR_BROADCAST=-1, ADDR_SERVER=-2, или ID клиента
    ///   [12..15]- from (int)         — ID отправителя
    /// 
    /// Payload: UTF-16LE кодировка (Encoding.Unicode)
    /// 
    /// Модель: pull/broker — данные запрашиваются вызовом GetData(),
    /// который отправляет MT_GETDATA серверу и читает один ответ.
    /// Нет фонового потока чтения (ReaderLoop/push-модель удалена).
    /// </summary>
    public class SalakhovaSocketClient
    {
        // --- Константы протокола ---
        public const int MT_CLOSE   = 0;
        public const int MT_DATA    = 1;
        public const int MT_START   = 2;
        public const int MT_STOP    = 3;
        public const int MT_QUIT    = 4;
        public const int MT_INFO    = 5;
        public const int MT_CONFIRM = 6;
        public const int MT_GETDATA = 7;
        public const int MT_NODATA  = 8;

        public const int ADDR_BROADCAST = -1;
        public const int ADDR_SERVER    = -2;

        private const int HeaderSize = 16;
        private const int DefaultPort = 12345;
        private const int ReceiveTimeoutMs = 5000;

        // --- Поля состояния ---
        private Socket socket;
        private int clientId;
        private volatile bool isConnected;

        // --- Синхронизация ---
        private readonly object writeLock = new object();

        // --- Публичные свойства ---
        public bool IsConnected => isConnected;
        public int ClientId => clientId;

        // ====================================================================
        //  PUBLIC API
        // ====================================================================

        /// <summary>
        /// Подключение к серверу Салаховой.
        /// После TCP-рукопожатия читает первый MT_CONFIRM для получения clientId.
        /// Фоновый поток чтения НЕ запускается — данные запрашиваются через GetData().
        /// Устанавливается ReceiveTimeout на сокете для предотвращения зависаний.
        /// </summary>
        public bool Connect(string host, int port = DefaultPort)
        {
            if (isConnected)
                throw new InvalidOperationException("Already connected.");

            try
            {
                socket = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp);
                socket.Connect(host, port);

                // Устанавливаем таймаут чтения, чтобы не зависнуть навсегда
                socket.ReceiveTimeout = ReceiveTimeoutMs;

                // --- Читаем первый MT_CONFIRM (handshake) ---
                byte[] headerBuf = ReadExact(HeaderSize);
                var (command, size, _, from) = UnpackHeader(headerBuf);

                if (command != MT_CONFIRM)
                {
                    socket.Close();
                    return false;
                }

                // Читаем payload с ID клиента
                string payload = "";
                if (size > 0)
                {
                    byte[] payloadBuf = ReadExact(size);
                    payload = Encoding.Unicode.GetString(payloadBuf);
                }

                if (!int.TryParse(payload, out clientId))
                {
                    socket.Close();
                    return false;
                }

                isConnected = true;
                return true;
            }
            catch
            {
                CleanupSocket();
                return false;
            }
        }

        /// <summary>
        /// Отключение от сервера.
        /// Отправляет MT_QUIT(ADDR_SERVER), затем закрывает сокет.
        /// </summary>
        public void Disconnect()
        {
            isConnected = false;

            try
            {
                Send(ADDR_SERVER, MT_QUIT, "");
            }
            catch
            {
                // Игнорируем ошибки при отправке QUIT
            }

            try
            {
                if (socket != null && socket.Connected)
                {
                    socket.Shutdown(SocketShutdown.Both);
                }
            }
            catch
            {
                // Игнорируем
            }

            CleanupSocket();
        }

        /// <summary>
        /// Отправка сообщения серверу.
        /// </summary>
        /// <param name="target">ADDR_BROADCAST, ADDR_SERVER или ID клиента</param>
        /// <param name="messageType">Тип сообщения (MT_DATA, MT_QUIT и т.д.)</param>
        /// <param name="text">Текст сообщения (может быть пустым)</param>
        public void Send(int target, int messageType, string text)
        {
            if (!isConnected)
                throw new InvalidOperationException("Not connected.");

            byte[] payload = string.IsNullOrEmpty(text)
                ? Array.Empty<byte>()
                : Encoding.Unicode.GetBytes(text);

            byte[] header = PackHeader(messageType, payload.Length, target, clientId);

            lock (writeLock)
            {
                socket.Send(header, SocketFlags.None);
                if (payload.Length > 0)
                {
                    socket.Send(payload, SocketFlags.None);
                }
            }
        }

        /// <summary>
        /// Синхронный запрос одного сообщения от сервера (pull-модель).
        /// Отправляет MT_GETDATA и читает один ответ.
        /// </summary>
        /// <param name="source">ID отправителя (from)</param>
        /// <param name="command">Тип сообщения (MT_DATA, MT_CONFIRM и т.д.)</param>
        /// <param name="target">Адресат (to)</param>
        /// <param name="text">Текст payload (может быть пустым)</param>
        /// <returns>true если есть данные (MT_DATA или MT_CONFIRM), false если данных нет (MT_NODATA)</returns>
        public bool GetData(out int source, out int command, out int target, out string text)
        {
            source  = 0;
            command = 0;
            target  = 0;
            text    = null;

            if (!isConnected)
                return false;

            try
            {
                // 1. Отправляем MT_GETDATA серверу (без payload)
                byte[] headerReq = PackHeader(MT_GETDATA, 0, ADDR_SERVER, clientId);

                lock (writeLock)
                {
                    socket.Send(headerReq, SocketFlags.None);
                }

                // 2. Читаем заголовок ответа
                byte[] headerResp = ReadExact(HeaderSize);
                var (msgType, size, to, from) = UnpackHeader(headerResp);

                // 3. Обрабатываем ответ
                string payload = "";
                if (size > 0)
                {
                    byte[] payloadBuf = ReadExact(size);
                    payload = Encoding.Unicode.GetString(payloadBuf);
                }

                command = msgType;
                source  = from;
                target  = to;
                text    = payload;

                if (msgType == MT_NODATA)
                {
                    return false;
                }

                // MT_DATA, MT_CONFIRM и любые другие — возвращаем true
                return true;
            }
            catch
            {
                // При ошибке чтения (разрыв соединения) — помечаем как отключённый
                isConnected = false;
                return false;
            }
        }

        // ====================================================================
        //  УПАКОВКА / РАСПАКОВКА ЗАГОЛОВКА
        // ====================================================================

        /// <summary>
        /// Упаковка 16-байтного заголовка.
        /// Порядок полей: messageType, size, to, from (little-endian int32).
        /// </summary>
        private static byte[] PackHeader(int messageType, int size, int to, int from)
        {
            byte[] buf = new byte[HeaderSize];
            Buffer.BlockCopy(BitConverter.GetBytes(messageType), 0, buf, 0, 4);
            Buffer.BlockCopy(BitConverter.GetBytes(size),       0, buf, 4, 4);
            Buffer.BlockCopy(BitConverter.GetBytes(to),         0, buf, 8, 4);
            Buffer.BlockCopy(BitConverter.GetBytes(from),       0, buf, 12, 4);
            return buf;
        }

        /// <summary>
        /// Распаковка 16-байтного заголовка.
        /// Возвращает (messageType, size, to, from).
        /// </summary>
        private static (int messageType, int size, int to, int from) UnpackHeader(byte[] buf)
        {
            int messageType = BitConverter.ToInt32(buf, 0);
            int size        = BitConverter.ToInt32(buf, 4);
            int to          = BitConverter.ToInt32(buf, 8);
            int from        = BitConverter.ToInt32(buf, 12);
            return (messageType, size, to, from);
        }

        // ====================================================================
        //  ВСПОМОГАТЕЛЬНЫЕ МЕТОДЫ
        // ====================================================================

        /// <summary>
        /// Точное чтение ровно N байт из сокета (блокирующее).
        /// </summary>
        private byte[] ReadExact(int count)
        {
            byte[] buffer = new byte[count];
            int offset = 0;

            while (offset < count)
            {
                int received = socket.Receive(buffer, offset, count - offset, SocketFlags.None);
                if (received == 0)
                    throw new SocketException((int)SocketError.ConnectionReset);

                offset += received;
            }

            return buffer;
        }

        /// <summary>
        /// Безопасная очистка и закрытие сокета.
        /// </summary>
        private void CleanupSocket()
        {
            try
            {
                socket?.Close();
            }
            catch
            {
                // Игнорируем
            }
            socket = null;
        }
    }
}
