#!/bin/bash
# Capture screenshots of tde-firewalld under Xvfb with the TDE window manager.
#
# Prerequisites (see the README): a Debian container with the TDE packages,
# tde-firewalld installed, a running system D-Bus and firewalld, Xvfb on :1,
# xdotool and ImageMagick. A user "tester" without polkit rights is used
# for the "authentication required" screenshot.
export DISPLAY=:1 PATH=/opt/trinity/bin:$PATH HOME=/root
export XDG_CONFIG_DIRS=/opt/trinity/etc/xdg:/etc/xdg XDG_DATA_DIRS=/opt/trinity/share:/usr/share
OUT=/shots; mkdir -p $OUT
pkill -x tde-firewalld; pkill -x tdecmshell.real; pkill -x kcontrol; sleep 1
pgrep -x twin >/dev/null || (twin >/dev/null 2>&1 &)
xsetroot -solid "#3a6ea5"
# stand-in so the "no polkit agent" banner does not appear in screenshots
pgrep -f "^/tmp/polkit-agent-tde" >/dev/null || { cp /bin/sleep /tmp/polkit-agent-tde; (/tmp/polkit-agent-tde 100000 &) ; }
sleep 2

shot() { # name window-pattern
  local id
  for i in $(seq 1 30); do id=$(xdotool search --onlyvisible --name "$2" | tail -1); [ -n "$id" ] && break; sleep 0.5; done
  if [ -z "$id" ]; then echo "MISSING window for $1"; return; fi
  sleep 1.5
  timeout 20 import -frame -window "$id" "$OUT/$1.png" && echo "saved $1"
}
key() { xdotool key --clearmodifiers "$@"; sleep 1.2; }
app() { pkill -x tde-firewalld; sleep 1.5; rm -f /root/.trinity/share/config/tde-firewalldrc; (tde-firewalld "$@" >/dev/null 2>&1 &); sleep 4; }

app --page status;                 shot status "Firewall Manager"
app --page zones --zone public;    shot zones-services "Firewall Manager"
key alt+p;                         shot zones-ports "Firewall Manager"
key alt+r;                         shot zones-rich-rules "Firewall Manager"
app --page zones --zone external;  key alt+d; shot zones-port-forwarding "Firewall Manager"
app --page zones --zone public --permanent; shot zones-permanent "Firewall Manager"
app --page services; key alt+s; xdotool type --delay 80 ssh; sleep 1; shot services "Firewall Manager"
app --page interfaces;             shot interfaces "Firewall Manager"
app --page zones --zone public;    key alt+p; key alt+a
xdotool type --delay 80 8443; sleep 0.5; shot add-port-dialog "Open Port"
key Escape
app --page zones --zone public;    key alt+r; key alt+a
key alt+e Down Down; shot rich-rule-dialog "Add Rich Rule"
key Escape
pkill -x tde-firewalld; sleep 1

# Trinity Control Center: Security & Privacy -> Firewall
(kcontrol >/dev/null 2>&1 &); sleep 7
W=$(xdotool search --onlyvisible --name "Control Center" | tail -1)
xdotool windowmove $W 0 0; xdotool windowsize $W 1100 800; sleep 1
xdotool mousemove 16 209 click 1; sleep 1; xdotool mousemove 96 249 click 1; sleep 5
shot kcontrol "Control Center"
pkill -x kcontrol; sleep 1

# a normal user whose authentication failed (no polkit session in the container)
su - tester -c "export DISPLAY=:1 PATH=/opt/trinity/bin:\$PATH; rm -f ~/.trinity/share/config/tde-firewalldrc; (tde-firewalld --page status >/dev/null 2>&1 &); sleep 7"
shot auth-required "Firewall Manager"
pkill -x tde-firewalld
