
## Сборка

Проект использует `CMake`.

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

## Запуск
**Сервер**
Запускается с указанием порта:

```bash
./server <port>
```

**Клиент**
Запускается с параметрами:

```bash
./client_verifier <n> <connections> <server_addr> <server_port>
```
- n — количество чисел в выражении 

- connections — количество параллельных TCP-соединений

- server_addr — IP-адрес сервера (например, 127.0.0.1)

- server_port — порт, на котором слушает сервер

## Пример
Успешный ответ:
```yaml
CORRECT: 12+3*4-5 => server: 19, expected: 19
```
Ошибка:
```yaml
FAIL: 8+9/2 => server: 10.5, expected: 12.5
```
