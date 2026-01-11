# [VIP] Respawn (Addition)

Плагин для Counter-Strike 2, предоставляющий расширенные возможности возрождения для VIP игроков.

## Автор
PattHs

## Описание
Этот плагин позволяет VIP игрокам возрождаться во время раунда с различными настройками и ограничениями.

## Функции
- Возрождение через команду `!respawn` или `/respawn`
- Автоматическое возрождение после смерти
- Настраиваемые задержки и лимиты возрождений
- Проверка на минимальное количество игроков для доступа к возрождению

## Параметры
- `Respawn` (int): Количество доступных возрождений
- `AutoRespawn` (bool): Включить автоматическое возрождение
- `NoRespawnDelay` (float): Время без задержки возрождения
- `NoAutoRespawnDelay` (float): Время без задержки авто-возрождения
- `RespawnDelay` (float): Задержка перед активацией возрождения
- `AutoRespawnDelay` (float): Задержка авто-возрождения
- `RespawnMinPlayers` (int): Минимальное количество игроков для активации возрождения после смерти
- `AutoRespawnMinPlayers` (int): Минимальное количество игроков для авто-возрождения
- `RespawnAccessMinPlayers` (int): Минимальное количество игроков для доступа к возрождению через команду
- `HPAfterRespawn` (int): Здоровье после возрождения через команду
- `HPAfterAutoRespawn` (int): Здоровье после авто-возрождения

## Настройка

### groups.ini
Добавьте в groups.ini следующие параметры:

```
"Respawn" "3" // Кол-во респавнов (учитывается как обычных, так и авто).
"AutoRespawn" "1" // Доступ к автореспавну
"NoRespawnDelay" "40" // Через сколько сек. респавн будет недоступен до след. раунда
"NoAutoRespawnDelay" "35" // Через сколько сек. автореспавн будет недоступен до след. раунда
"RespawnDelay" "7" // Через сколько сек. после смерти респавн будет доступен
"AutoRespawnDelay" "3" // Через сколько сек. после смерти произойдет автореспавн
"RespawnMinPlayers" "2" // Сколько мин. игроков нужно для работы респавна
"AutoRespawnMinPlayers" "5" // Сколько мин. игроков нужно для работы автореспавна
"RespawnAccessMinPlayers" "10" // Минимальное количество игроков для доступа к возрождению через команду
"HPAfterRespawn" "75" // Кол-во здоровья после респавна
"HPAfterAutoRespawn" "50" // Кол-во здоровья после автореспавна
```

### vip.phrases.txt
Добавьте в файл vip.phrases.txt следующие фразы:

```
"Respawn"
{
    "en"    "Respawn"
    "ru"    "Респавн"
}
"AutoRespawn"
{
    "en"    "AutoRespawn"
    "ru"    "АвтоРеспавн"
}
"RespawnAvailable"
{
    "en" "Respawns available: %d"
    "ru" "Респавнов доступно: %d"
}
"RespawnRemaining"
{
    "en" "Respawns remaining: %d"
    "ru" "Респавнов осталось: %d"
}
"RespawnIsNotAvailable"
{
    "en" "Respawn is unavailable."
    "ru" "Респавн недоступен."
}
"RespawnIsNoLongerAvailable"
{
    "en" "Respawn is unavailable."
    "ru" "Респавн больше недоступен."
}
"AutoRespawnIsNoLongerAvailable"
{
    "en" "Autorespawn is no longer available."
    "ru" "Автореспавн больше недоступен."
}
"NotEnoughOnlinePlayersForRespawn"
{
    "en" "Not enough players online to respawn"
    "ru" "Недостаточно игроков онлайн для респавна"
}
"NotEnoughOnlinePlayersForAutoRespawn"
{
    "en" "Not enough players online for auto-respawn"
    "ru" "Недостаточно игроков онлайн для автореспавна"
}
```

## Установка
1. Скачайте и установите Metamod для CS2
2. Скачайте плагин и разместите файлы в соответствующие папки
3. Перезагрузите сервер

## Сборка
Для сборки плагина требуется AMBuild.

```bash
python configure.py
ambuild
```

## Лицензия
Public

## Ссылка
[https://nova-hosting.ru?ref=TNC36I97](https://nova-hosting.ru?ref=TNC36I97)
