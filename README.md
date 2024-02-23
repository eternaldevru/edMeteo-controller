[![Foo](https://img.shields.io/badge/version-1.0-brightgreen)](#versions)
[![Foo](https://img.shields.io/badge/website-eternaldev.ru-red)](https://eternaldev.ru)
[![Foo](https://img.shields.io/badge/Telegram-eternaldev__ru-blue)](https://t.me/eternaldev_ru)
[![Foo](https://img.shields.io/youtube/views/HI4E4ZCWdIk?style=social)](https://www.youtube.com/watch?v=HI4E4ZCWdIk)

# edMeteo - метеостанция для "Народного мониторинга"

Сборку проекта [смотрите на YouTube](https://www.youtube.com/watch?v=HI4E4ZCWdIk) 

## Зависимости
Для компиляции скетча потребуются следующие библиотеки:
- В поле "Дополнительные ссылки Менеджера плат" вставить https://raw.githubusercontent.com/dbuezas/lgt8fx/master/package_lgt8fx_index.json и нажать "Ок"
- Перейти в "Инструменты" -> "Плата" -> "Менеджер плат…"
- Начать вводить в поле поиска "lgt8". Выбрать и установить "LGT8fx Boards"
- Перейти в "Инструменты" -> "Плата". В списке плат должно появиться семейство плат Logic Green. Выбираем "LGT8F328"
- Также, необходимо установить драйвер из папки "driver" проекта, если это не произошло автоматически


- microDS18B20 https://github.com/GyverLibs/microDS18B20
- GyverBME280 https://github.com/GyverLibs/GyverBME280
- EtherCard https://github.com/njh/EtherCard
- LiquidCrystal_I2C https://github.com/johnrickman/LiquidCrystal_I2C

## Схема
Схема устройства представлена ниже:

![scheme](/docs/circuit.png)

<a id="versions"></a>
## Версии
- v1.0