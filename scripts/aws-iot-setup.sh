#!/usr/bin/env bash
# =============================================================================
# AWS IoT Core + Timestream provisioning for Taktflow Pi edge gateway
#
# Creates: IoT Thing, X.509 certs, IoT policy, Timestream DB+table,
#          IAM role, IoT Rule (MQTT → Timestream).
#
# Prerequisites:
#   - AWS CLI v2 installed and configured (aws configure)
#   - jq installed
#
# Usage:
#   ./scripts/aws-iot-setup.sh
#
# Output:
#   - Certificates written to docker/certs/
#   - AWS_IOT_ENDPOINT printed for .env file
# =============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Configuration
THING_NAME="${THING_NAME:-taktflow-pi-001}"
POLICY_NAME="taktflow-telemetry-policy"
REGION="${AWS_REGION:-eu-central-1}"
CERT_DIR="$REPO_ROOT/docker/certs"
DB_NAME="taktflow-telemetry"
TABLE_NAME="vehicle-data"
ROLE_NAME="taktflow-iot-timestream-role"

echo "=== Taktflow AWS IoT Core Setup ==="
echo "Thing:     $THING_NAME"
echo "Region:    $REGION"
echo "Cert dir:  $CERT_DIR"
echo ""

# Create cert directory
mkdir -p "$CERT_DIR"
chmod 700 "$CERT_DIR"

# ---- Step 1: Create IoT Thing ----
echo "[1/8] Creating IoT Thing: $THING_NAME"
aws iot create-thing \
    --thing-name "$THING_NAME" \
    --region "$REGION" \
    2>/dev/null || echo "  (already exists)"

# ---- Step 2: Create and download certificates ----
echo "[2/8] Creating X.509 certificates"
CERT_RESPONSE=$(aws iot create-keys-and-certificate \
    --set-as-active \
    --certificate-pem-outfile "$CERT_DIR/device.cert.pem" \
    --public-key-outfile "$CERT_DIR/device.public.key" \
    --private-key-outfile "$CERT_DIR/device.private.key" \
    --region "$REGION" \
    --output json)

CERT_ARN=$(echo "$CERT_RESPONSE" | jq -r '.certificateArn')
echo "  Certificate ARN: $CERT_ARN"

chmod 600 "$CERT_DIR"/*.pem "$CERT_DIR"/*.key

# ---- Step 3: Download Amazon Root CA ----
echo "[3/8] Downloading Amazon Root CA"
curl -sS -o "$CERT_DIR/root-CA.pem" \
    https://www.amazontrust.com/repository/AmazonRootCA1.pem
chmod 600 "$CERT_DIR/root-CA.pem"

# ---- Step 4: Create IoT Policy ----
echo "[4/8] Creating IoT policy: $POLICY_NAME"
ACCOUNT_ID=$(aws sts get-caller-identity --query 'Account' --output text)

aws iot create-policy \
    --policy-name "$POLICY_NAME" \
    --policy-document "{
        \"Version\": \"2012-10-17\",
        \"Statement\": [
            {
                \"Effect\": \"Allow\",
                \"Action\": \"iot:Connect\",
                \"Resource\": \"arn:aws:iot:${REGION}:${ACCOUNT_ID}:client/${THING_NAME}\"
            },
            {
                \"Effect\": \"Allow\",
                \"Action\": \"iot:Publish\",
                \"Resource\": [
                    \"arn:aws:iot:${REGION}:${ACCOUNT_ID}:topic/vehicle/telemetry\",
                    \"arn:aws:iot:${REGION}:${ACCOUNT_ID}:topic/vehicle/dtc/*\",
                    \"arn:aws:iot:${REGION}:${ACCOUNT_ID}:topic/vehicle/alerts\"
                ]
            }
        ]
    }" \
    --region "$REGION" \
    2>/dev/null || echo "  (already exists)"

# ---- Step 5: Attach policy and thing to certificate ----
echo "[5/8] Attaching policy and thing to certificate"
aws iot attach-policy \
    --policy-name "$POLICY_NAME" \
    --target "$CERT_ARN" \
    --region "$REGION"

aws iot attach-thing-principal \
    --thing-name "$THING_NAME" \
    --principal "$CERT_ARN" \
    --region "$REGION"

# ---- Step 6: Create Timestream database + table ----
echo "[6/8] Creating Timestream database: $DB_NAME"
aws timestream-write create-database \
    --database-name "$DB_NAME" \
    --region "$REGION" \
    2>/dev/null || echo "  (already exists)"

echo "  Creating Timestream table: $TABLE_NAME"
aws timestream-write create-table \
    --database-name "$DB_NAME" \
    --table-name "$TABLE_NAME" \
    --retention-properties \
        "MemoryStoreRetentionPeriodInHours=24,MagneticStoreRetentionPeriodInDays=365" \
    --region "$REGION" \
    2>/dev/null || echo "  (already exists)"

# ---- Step 7: Create IAM role for IoT → Timestream ----
echo "[7/8] Creating IAM role: $ROLE_NAME"
TRUST_POLICY='{
    "Version": "2012-10-17",
    "Statement": [{
        "Effect": "Allow",
        "Principal": {"Service": "iot.amazonaws.com"},
        "Action": "sts:AssumeRole"
    }]
}'

ROLE_ARN=$(aws iam create-role \
    --role-name "$ROLE_NAME" \
    --assume-role-policy-document "$TRUST_POLICY" \
    --query 'Role.Arn' \
    --output text \
    --region "$REGION" \
    2>/dev/null || aws iam get-role \
        --role-name "$ROLE_NAME" \
        --query 'Role.Arn' \
        --output text)

echo "  Role ARN: $ROLE_ARN"

# Attach Timestream write permissions
aws iam put-role-policy \
    --role-name "$ROLE_NAME" \
    --policy-name "taktflow-timestream-write" \
    --policy-document "{
        \"Version\": \"2012-10-17\",
        \"Statement\": [{
            \"Effect\": \"Allow\",
            \"Action\": [
                \"timestream:WriteRecords\",
                \"timestream:DescribeEndpoints\"
            ],
            \"Resource\": \"arn:aws:timestream:${REGION}:${ACCOUNT_ID}:database/${DB_NAME}/table/${TABLE_NAME}\"
        }]
    }"

# Wait for IAM propagation
echo "  Waiting for IAM propagation (10s)..."
sleep 10

# ---- Step 8: Create IoT Rule (MQTT → Timestream) ----
echo "[8/8] Creating IoT Rule: taktflow_to_timestream"
aws iot create-topic-rule \
    --rule-name "taktflow_to_timestream" \
    --topic-rule-payload "{
        \"sql\": \"SELECT * FROM 'vehicle/#'\",
        \"actions\": [{
            \"timestream\": {
                \"roleArn\": \"${ROLE_ARN}\",
                \"databaseName\": \"${DB_NAME}\",
                \"tableName\": \"${TABLE_NAME}\",
                \"dimensions\": [
                    {\"name\": \"device_id\", \"value\": \"${THING_NAME}\"},
                    {\"name\": \"topic\", \"value\": \"\${topic()}\"}
                ]
            }
        }]
    }" \
    --region "$REGION" \
    2>/dev/null || echo "  (already exists)"

# ---- Print endpoint ----
ENDPOINT=$(aws iot describe-endpoint \
    --endpoint-type iot:Data-ATS \
    --region "$REGION" \
    --query 'endpointAddress' \
    --output text)

echo ""
echo "=== Setup Complete ==="
echo ""
echo "AWS IoT Endpoint: $ENDPOINT"
echo ""
echo "Add to docker/.env:"
echo "  AWS_IOT_ENDPOINT=$ENDPOINT"
echo ""
echo "Certificates saved to: $CERT_DIR/"
echo "  root-CA.pem, device.cert.pem, device.private.key"
echo ""
echo "Deploy to Pi:"
echo "  cd docker && docker compose -f docker-compose.pi.yml up --build -d"
