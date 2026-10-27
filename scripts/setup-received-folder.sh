#!/usr/bin/env bash
# One-time setup for the shared "Received files" folder on a Linux (systemd) Vibebeam host.
#
# The host service is locked down: it runs as its own user, cannot see home folders and can only
# write to /var/lib/vibepollo, which is private. That is why files sent from Vibelight were hard to
# reach. This script makes one extra folder, /var/lib/vibepollo-received, that the service may write
# to and that you (and only you and the service) can open, move files out of and delete from.
#
# It does NOT add you to the "vibepollo" group, which can run the privileged host binary. It uses a
# new group, vibepollo-received, that controls this one folder only.
set -euo pipefail

if [ "$(id -u)" -eq 0 ]; then
  echo "Run this as your normal user (it uses sudo where needed), not as root." >&2
  exit 1
fi

DIR=/var/lib/vibepollo-received
GROUP=vibepollo-received
DROPIN_DIR=/etc/systemd/system/vibepollo.service.d
DROPIN=$DROPIN_DIR/received-files.conf

sudo groupadd -f "$GROUP"
sudo usermod -aG "$GROUP" "$USER"
sudo install -d -m 2770 -o vibepollo -g "$GROUP" "$DIR"

sudo install -d -m 0755 "$DROPIN_DIR"
printf '[Service]\nReadWritePaths=%s\n' "$DIR" | sudo tee "$DROPIN" >/dev/null
sudo systemctl daemon-reload

# Move anything received earlier into the new folder
OLD=/var/lib/vibepollo/received-files
if sudo test -d "$OLD" && [ -n "$(sudo ls -A "$OLD" 2>/dev/null)" ]; then
  sudo sh -c "mv -n '$OLD'/* '$DIR'/ && chown -R vibepollo:$GROUP '$DIR' && chmod -R g+rwX '$DIR'"
  echo "Moved earlier received files into $DIR"
fi

# A shortcut so the folder shows up in Documents
DOCS=$(xdg-user-dir DOCUMENTS 2>/dev/null || echo "$HOME/Documents")
mkdir -p "$DOCS"
ln -sfn "$DIR" "$DOCS/Received files"

echo "Restarting the Vibebeam service (this ends any stream in progress)..."
sudo systemctl restart vibepollo.service

cat <<EOT

Done.
- Received files are now saved in $DIR
- A shortcut named "Received files" is in $DOCS
- Log out and back in once so your account picks up the new group; until then use sudo to open the folder.
EOT
