# Сборка

Использован Windows PSPDEV [dmang-dev/pspdev-win v2](https://github.com/dmang-dev/pspdev-win/releases/tag/v2),
GCC 15.2.0. SHA-256 использованного SDK-архива:
`037f902acd90cfcf4711aeee6462479a6d0eabee6faf93ff44fe1081e653d512`.
Другие toolchain и ОС требуют отдельной проверки.

Нужны Python 3, Pillow, numpy, PSPDEV с FreeType, bzip2, libpng, zlib и SDK stubs.
Для Windows также нужен совместимый MSYS runtime, например Git for Windows.

```powershell
python -m pip install -r requirements.txt
$env:PSPDEV = 'C:/path/to/pspdev'
$env:Path = "C:/Program Files/Git/usr/bin;$env:Path"
python build.py
python make_music.py
```

Результат: `EBOOT.PBP`. `flow.elf`, `flow.prx`, `PARAM.SFO` — промежуточные файлы.
Скрипт компилирует vendored Mbed TLS, генерирует шрифтовый атлас/иконку/кнопки,
встраивает японский шрифт и проверяет резерв памяти и TLS-часы в итоговом ELF.
Для HTTPS не отключаются проверки сертификата и имени узла.

При упаковке положи EBOOT, `cacert.pem`, `music.wav`, `LICENSE` и `licenses/`
в `PSP/GAME/FLOWGO/`. Приложение создаёт рабочие папки само.
Шрифтовые лицензии из `assets/` тоже должны сопровождать бинарную поставку.
Не включай свои логи, настройки, приватные источники и entropy seed.

## Локальные проверки

`tests/source_switch.c` — host harness источников/очереди. Собирается обычным C
компилятором вместе с `sources.c`, `scheduler.c`, `core.c`. Запускай в отдельной
временной директории с пустыми `disk/` и `downloads/`: тест пишет файлы там.

`tests/harness.c` — парсер и ZIP. Подробнее в `tests/README.txt`.
Нужны исходники zlib 1.3.1 для host-компиляции MiniZip; PSP-библиотека zlib
не подходит для запуска теста на ПК. Эмулятор для этих проверок не используется.

Исправления из host-тестов не заменяют проверку на PSP Go.
