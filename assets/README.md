# MAT OS — Assets

Этот каталог содержит визуальные ресурсы для MAT OS.

## Содержимое

```
assets/
├── wallpaper.png          # Обои рабочего стола (1920×1080, синий градиент)
├── start_button.png       # Изображение кнопки «Пуск» (48×48, 4-цветный логотип Windows)
└── icons/
    ├── folder.png         # Иконка папки (32×32)
    ├── file.png           # Иконка файла (32×32)
    ├── computer.png       # Иконка «Мой компьютер»
    ├── network.png        # Иконка сетевого окружения
    └── trash.png          # Иконка корзины
```

## Генерация ресурсов

Для генерации базовых ресурсов без художественных PNG-файлов:

```bash
# Создать простые PNG-иконки через ImageMagick
apt-get install -y imagemagick

# Обои (синий градиент 1920×1080)
convert -size 1920x1080 gradient:"#032D60-#0078D7" assets/wallpaper.png

# Кнопка Пуск (4-цветный квадрат)
convert -size 48x48 xc:none \
  -fill '#F65314' -draw 'rectangle 2,2 22,22' \
  -fill '#7CBB00' -draw 'rectangle 26,2 46,22' \
  -fill '#00A1F1' -draw 'rectangle 2,26 22,46' \
  -fill '#FFBB00' -draw 'rectangle 26,26 46,46' \
  assets/start_button.png

# Иконка папки
convert -size 32x32 xc:'#0078D7' \
  -fill '#FFB900' -draw 'roundrectangle 2,8 30,28 3,3' \
  assets/icons/folder.png

# Иконка файла
convert -size 32x32 xc:none \
  -fill '#FFFFFF' -stroke '#AAAAAA' -strokewidth 1 \
  -draw 'polygon 2,2 22,2 30,10 30,30 2,30' \
  assets/icons/file.png
```

## Использование в коде

Ресурсы копируются в `/opt/matos-shell/assets/` при установке системы.

В коде оболочки путь задаётся через переменную окружения:
```
MATOS_ASSETS_DIR=/opt/matos-shell/assets
```
