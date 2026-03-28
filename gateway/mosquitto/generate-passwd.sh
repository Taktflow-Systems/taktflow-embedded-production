#!/usr/bin/env bash
# Generate Mosquitto password file for the taktflow MQTT user.
# Run once: ./generate-passwd.sh [password]
# The default password is for development only — override in production.
set -euo pipefail

PASSWD_FILE="$(dirname "$0")/passwd"
MQTT_USER="taktflow"
MQTT_PASS="${1:-${MQTT_PASSWORD:-taktflow-dev}}"

if command -v mosquitto_passwd >/dev/null 2>&1; then
    mosquitto_passwd -b -c "$PASSWD_FILE" "$MQTT_USER" "$MQTT_PASS"
    echo "Password file generated: $PASSWD_FILE"
else
    # Fallback: use Docker to generate the file
    docker run --rm -v "$(dirname "$0"):/mosquitto/config" \
        eclipse-mosquitto:2 \
        mosquitto_passwd -b -c /mosquitto/config/passwd "$MQTT_USER" "$MQTT_PASS"
    echo "Password file generated via Docker: $PASSWD_FILE"
fi
