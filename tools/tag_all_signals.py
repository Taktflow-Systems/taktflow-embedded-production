#!/usr/bin/env python3
"""
Generate BA_ attribute entries for ALL untagged signals in the production DBC.

Ownership rules:
  - E2E fields (DataID, AliveCounter, CRC8) → "BSW E2E" on sender ECU
  - Heartbeat fields (ECU_ID, OperatingMode, FaultStatus, AliveCounter) → "Swc_Heartbeat" on sender
  - SC_Status fields → "SC firmware"
  - Command/status fields → SWC that generates the value
  - All fields get InitValue = 0 unless special (RelayState=1, BatteryStatus=2 normal)

Output: BA_ lines to append to the DBC file.
"""

import cantools
import sys

db = cantools.database.load_file("gateway/taktflow_vehicle.dbc")

# Map: sender ECU → heartbeat SWC
HB_SWC = {
    "CVC": "Swc_Heartbeat",
    "FZC": "Swc_Heartbeat",
    "RZC": "Swc_Heartbeat",
    "BCM": "Swc_BcmMain",
    "ICU": "Swc_Dashboard",
    "TCU": "Swc_DataAggregator",
}

# Map: sender ECU → Com SWC (packs CAN frames)
COM_SWC = {
    "CVC": "Swc_CvcCom",
    "FZC": "Swc_FzcCom",
    "RZC": "Swc_RzcCom",
    "SC":  "SC firmware (sc_can.c)",
    "BCM": "Swc_BcmCan",
    "ICU": "Swc_Dashboard",
    "TCU": "Swc_UdsServer",
}

# Map: specific signal → owner SWC (overrides default)
SIGNAL_OWNER = {
    # CVC
    "VehSt_TorqueLimit": "Swc_VehicleState",
    "VehSt_SpeedLimit": "Swc_VehicleState",
    "TrqRq_Direction": "Swc_Pedal",
    "TrqRq_PedalPosition1": "Swc_Pedal",
    "TrqRq_PedalPosition2": "Swc_Pedal",
    "TrqRq_PedalFault": "Swc_Pedal",
    "StrCmd_SteerRateLimit": "Swc_CvcCom",
    "StrCmd_VehicleState": "Swc_VehicleState",
    "BrkCmd_BrakeMode": "Swc_CvcCom",
    "BrkCmd_VehicleState": "Swc_VehicleState",
    # FZC
    "StrSt_ActualAngle": "Swc_Steering",
    "StrSt_CommandedAngle": "Swc_Steering",
    "StrSt_SteerFaultStatus": "Swc_Steering",
    "StrSt_SteerMode": "Swc_Steering",
    "StrSt_ServoCurrent_mA": "Swc_Steering",
    "BrkSt_BrakePosition": "Swc_Brake",
    "BrkSt_BrakeCommandEcho": "Swc_Brake",
    "BrkSt_ServoCurrent_mA": "Swc_Brake",
    "BrkSt_BrakeFaultStatus": "Swc_Brake",
    "BrkSt_BrakeMode": "Swc_Brake",
    "BrkFlt_FaultType": "Swc_Brake",
    "BrkFlt_CommandedBrake": "Swc_Brake",
    "BrkFlt_MeasuredBrake": "Swc_Brake",
    "MtrCut_RequestType": "Swc_FzcSafety",
    "MtrCut_Reason": "Swc_FzcSafety",
    "LidarD_Distance_cm": "Swc_Lidar",
    "LidarD_SignalStrength": "Swc_Lidar",
    "LidarD_ObstacleZone": "Swc_Lidar",
    "LidarD_SensorStatus": "Swc_Lidar",
    # RZC
    "MtrSt_TorqueEcho": "Swc_Motor",
    "MtrSt_MotorSpeed_RPM": "Swc_Encoder",
    "MtrSt_MotorDirection": "Swc_Motor",
    "MtrSt_MotorEnable": "Swc_Motor",
    "MtrCur_CurrentDirection": "Swc_CurrentMonitor",
    "MtrCur_MotorEnable": "Swc_Motor",
    "MtrCur_TorqueEcho": "Swc_Motor",
    "MtrTmp_WindingTemp2_C": "Swc_TempMonitor",
    "MtrTmp_DeratingPercent": "Swc_TempMonitor",
    # BCM
    "BdyCmd_HeadlightCmd": "Swc_CvcCom",
    "BdyCmd_TailLightCmd": "Swc_CvcCom",
    "BdyCmd_HazardCmd": "Swc_CvcCom",
    "BdyCmd_TurnSignalCmd": "Swc_CvcCom",
    "BdyCmd_DoorLockCmd": "Swc_CvcCom",
    "LtSt_HeadlightOn": "Swc_Lights",
    "LtSt_TailLightOn": "Swc_Lights",
    "LtSt_FogLightOn": "Swc_Lights",
    "LtSt_BrakeLightOn": "Swc_Lights",
    "LtSt_HeadlightLevel": "Swc_Lights",
    "IndSt_LeftIndicator": "Swc_Indicators",
    "IndSt_RightIndicator": "Swc_Indicators",
    "IndSt_HazardActive": "Swc_Indicators",
    "IndSt_BlinkState": "Swc_Indicators",
    "DlkSt_FrontLeftLock": "Swc_DoorLock",
    "DlkSt_FrontRightLock": "Swc_DoorLock",
    "DlkSt_RearLeftLock": "Swc_DoorLock",
    "DlkSt_RearRightLock": "Swc_DoorLock",
    "DlkSt_CentralLock": "Swc_DoorLock",
    # DTC
    "DtcBc_DTC_Status": "BSW Dem module",
    "DtcBc_ECU_Source": "BSW Dem module",
    "DtcBc_OccurrenceCount": "BSW Dem module",
    "DtcBc_FreezeFrame0": "BSW Dem module",
    "DtcBc_FreezeFrame1": "BSW Dem module",
    # SC
    "ScSt_AliveCounter": "SC firmware (sc_monitoring.c)",
    "ScSt_CRC8": "SC firmware (sc_monitoring.c)",
    "ScSt_Mode": "SC firmware (sc_state.c)",
    "ScSt_FaultFlags": "SC firmware (sc_relay.c)",
    "ScSt_ECU_Health": "SC firmware (sc_selftest.c)",
    "ScSt_FaultReason": "SC firmware (sc_relay.c)",
}

# Special init values (everything else = 0)
INIT_VALUES = {
    "ScSt_RelayState": 1,       # 1 = energized (safe default)
    "BatSt_BatteryStatus": 2,   # 2 = normal
    "BatSt_BatteryVoltage_mV": 12600,
    "MtrTmp_WindingTemp1_C": 250,  # 25.0 degC (0.1 scale)
}

lines = []

for msg in sorted(db.messages, key=lambda m: m.frame_id):
    sender = msg.senders[0] if msg.senders else "Unknown"
    for sig in msg.signals:
        # Skip already tagged
        already_tagged = False
        try:
            if sig.dbc.attributes.get("Owner"):
                already_tagged = True
        except Exception:
            pass
        if already_tagged:
            continue

        # Determine owner
        if sig.name in SIGNAL_OWNER:
            owner = SIGNAL_OWNER[sig.name]
        elif "E2E_DataID" in sig.name or "E2E_AliveCounter" in sig.name or "E2E_CRC8" in sig.name:
            owner = "BSW E2E on %s" % sender
        elif "AliveCounter" in sig.name or "ECU_ID" in sig.name:
            owner = HB_SWC.get(sender, "Swc_Heartbeat")
        elif "OperatingMode" in sig.name or "FaultStatus" in sig.name:
            owner = HB_SWC.get(sender, "Swc_Heartbeat")
        else:
            owner = COM_SWC.get(sender, "Unknown")

        # Determine ProducedBy (who packs the CAN frame)
        produced_by = COM_SWC.get(sender, sender)

        # InitValue
        init_val = INIT_VALUES.get(sig.name, 0)

        lines.append('BA_ "Owner" SG_ %d %s "%s";' % (msg.frame_id, sig.name, owner))
        lines.append('BA_ "ProducedBy" SG_ %d %s "%s";' % (msg.frame_id, sig.name, produced_by))
        lines.append('BA_ "InitValue" SG_ %d %s %d;' % (msg.frame_id, sig.name, init_val))

print("# Generated BA_ entries for %d untagged signals" % (len(lines) // 3))
print("# Append to gateway/taktflow_vehicle.dbc before the final blank line")
print()
for line in lines:
    print(line)
