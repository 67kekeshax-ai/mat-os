#!/bin/bash
# MAT OS — Заглушка «Настройки» (запускается из меню Пуск)
# В будущем будет заменена полноценным GUI-приложением на C++.

x-terminal-emulator -e bash -c "
echo '════════════════════════════════════════════════════'
echo '           MAT OS — Панель управления              '
echo '════════════════════════════════════════════════════'
echo ''
echo ' [1] Сеть (NetworkManager TUI)'
echo ' [2] Пользователи и пароли'
echo ' [3] Дата и время'
echo ' [4] Язык и регион'
echo ' [5] Информация о системе'
echo ' [Q] Выход'
echo ''
read -p ' Ваш выбор: ' choice
case \$choice in
  1) nmtui ;;
  2) passwd ;;
  3) date ;;
  4) locale ;;
  5) cat /etc/matos-edition; uname -a ;;
  *) ;;
esac
"
