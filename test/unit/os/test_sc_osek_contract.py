"""Host contract tests for S-OS-40 SC scheduling and watchdog ownership."""
from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[3]
MAIN = (ROOT / "firmware/ecu/sc/src/sc_main.c").read_text(encoding="utf-8")
CFG = (ROOT / "firmware/ecu/sc/src/sc_os_cfg.c").read_text(encoding="utf-8")
MAKE = (ROOT / "firmware/platform/tms570/Makefile.tms570").read_text(encoding="utf-8")
SCHEDULER = (ROOT / "firmware/bsw/os/bootstrap/src/Os_Scheduler.c").read_text(encoding="utf-8")
TMS_HW = (ROOT / "firmware/platform/tms570/src/Os_Port_Tms570_Hw.c").read_text(encoding="utf-8")
SC_TMS_HW = (ROOT / "firmware/platform/tms570/src/sc_hw_tms570.c").read_text(encoding="utf-8")
SC_ESM = (ROOT / "firmware/ecu/sc/src/sc_esm.c").read_text(encoding="utf-8")
TMS_TARGET = (ROOT / "firmware/platform/tms570/src/Os_Port_Tms570_Target.c").read_text(encoding="utf-8")
TASK_BINDING = (ROOT / "firmware/bsw/os/bootstrap/port/src/Os_Port_TaskBinding.c").read_text(encoding="utf-8")
TMS_ASM = (ROOT / "firmware/platform/tms570/src/Os_Port_Tms570_Asm.S").read_text(encoding="utf-8")
TMS_STARTUP = (ROOT / "firmware/ecu/sc/src/sc_startup.S").read_text(encoding="utf-8")
HAL_ESM = (ROOT / "firmware/ecu/sc/halcogen/source/HL_esm.c").read_text(encoding="utf-8")
HAL_VIM = (ROOT / "firmware/ecu/sc/halcogen/source/HL_sys_vim.c").read_text(encoding="utf-8")


def _function_body(name: str) -> str:
    start = MAIN.index(f"void {name}(void)")
    end = MAIN.index("\n}", start)
    return MAIN[start:end]


def test_only_highest_priority_safety_task_owns_watchdog_feed():
    assert "SC_TASK_MAIN_PRIORITY ((uint8)0u)" in (
        ROOT / "firmware/ecu/sc/include/sc_os_cfg.h"
    ).read_text(encoding="utf-8")
    assert MAIN.count("SC_Watchdog_Feed(") == 1
    assert "SC_Watchdog_Feed(all_checks_ok);" in _function_body("SC_Task_Main")
    assert "SC_Watchdog_Feed" not in _function_body("SC_Task_Idle")
    # Prove the kernel interpretation, not merely the configured numbers:
    # selection scans priority zero upward and returns the first ready task.
    select = SCHEDULER[SCHEDULER.index("os_select_next_ready_task"):]
    assert re.search(r"for \(priority = 0u; priority < OS_MAX_PRIORITIES; priority\+\+\)", select)
    assert select.index("for (priority = 0u") < select.index("return selected_task;")


def test_verified_sequence_precedes_watchdog_and_covers_fail_closed_inputs():
    body = _function_body("SC_Task_Main")
    ordered = [
        "SC_CAN_Receive();", "SC_Heartbeat_Monitor();",
        "SC_Plausibility_Check();", "SC_CreepGuard_Check();",
        "SC_Relay_CheckTriggers();", "SC_Monitoring_Update();",
        "SC_LED_Update();", "SC_CAN_MonitorBus();",
        "SC_SelfTest_Runtime();", "SC_Watchdog_Feed(all_checks_ok);",
    ]
    offsets = [body.index(token) for token in ordered]
    assert offsets == sorted(offsets)
    feed = body.index("SC_Watchdog_Feed(all_checks_ok);")
    for check in ("SC_SelfTest_StackCanaryOk()", "SC_SelfTest_IsHealthy()",
                  "SC_CAN_IsBusOff()", "SC_ESM_IsErrorActive()"):
        assert body.rindex(check) < feed
    assert "sequence_step != 9u" in body
    assert "sc_hw_cycle_time_us() - cycle_start_us) > 5000u" in body


def test_alarm_and_stack_configuration_is_complete():
    assert re.search(r'\{ "SC_Safety".*SC_TASK_MAIN_PRIORITY.*NON \}', CFG)
    assert re.search(r'\{ "SC_Idle".*OS_MAX_PRIORITIES - 1u.*FULL \}', CFG, re.S)
    assert '.Alarms = sc_alarms, .AlarmCount = SC_ALARM_COUNT' in CFG
    assert '.TaskStacks = sc_task_stacks, .TaskStackCount = SC_TASK_COUNT' in CFG
    assert 'SetRelAlarm(SC_ALARM_MAIN_ID, 1u, 1u)' in MAIN


def test_default_mode_autostarts_idle_for_first_task_launch():
    os_header = (ROOT / "firmware/bsw/os/bootstrap/include/Os.h").read_text(encoding="utf-8")
    core = (ROOT / "firmware/bsw/os/bootstrap/src/Os_Core.c").read_text(encoding="utf-8")
    assert "OSDEFAULTAPPMODE        ((AppModeType)0u)" in os_header
    assert "((uint32)1u << Mode)" in core
    # AutostartMask is a bit mask, not the numeric AppModeType. Default mode
    # zero therefore requires bit zero (1u), otherwise StartOS has no task.
    assert "((uint32)1u << OSDEFAULTAPPMODE), FALSE, FULL" in CFG
    assert "[OSEK-START] FAIL - StartOS returned" in MAIN


def test_tms570_build_links_kernel_and_target_port():
    assert "-DUSE_OSEK" in MAKE
    assert "$(wildcard $(OS_DIR)/src/*.c)" in MAKE
    assert "Os_Port_TaskBinding.c" in MAKE
    assert "Os_Port_Tms570_Hw.c" in MAKE
    assert "Os_Port_Tms570_Target.c" in MAKE
    assert "Os_Port_Tms570_Asm.S" in MAKE
    assert "-DSC_ESM_ENABLED" in MAKE


def test_tms570_bringup_is_opt_in_current_and_observable():
    assert "ifdef BRINGUP" in MAKE
    assert "-DOS_BOOTSTRAP_BRINGUP" in MAKE
    assert "Os_Port_Tms570_Bringup.c" in MAKE
    assert "build/tms570-bringup" in MAKE
    assert "sc_osek_bringup.elf" in MAKE
    assert "-bringup-" in MAKE
    assert "Os_Port_Tms570_BringupAll();" in MAIN
    assert "[OSEK-SAFE] relay held de-energized" in MAIN
    assert "[OSEK-ALARM] SC_10ms armed" in MAIN
    assert "[OSEK-TASK] SC_Safety activated by alarm" in MAIN
    bringup = (ROOT / "firmware/platform/tms570/src/Os_Port_Tms570_Bringup.c").read_text(encoding="utf-8")
    for marker in ("[BRINGUP-1]", "[BRINGUP-2]", "[BRINGUP-3]",
                   "[BRINGUP-4]", "[BRINGUP-5]", "[BRINGUP-6]",
                   "[BRINGUP-SUMMARY] ALL PASS"):
        assert marker in bringup


def test_fiq_bringup_rearms_both_compares_after_test5_teardown():
    bringup = (ROOT / "firmware/platform/tms570/src/Os_Port_Tms570_Bringup.c").read_text(encoding="utf-8")
    body = bringup[bringup.index("static boolean bringup_test_fiq_ownership(void)"):]
    body = body[:body.index("\n}")]
    assert "counterNow = rtiREG1->CNT[0u].FRCx;" in body
    assert "CMP[0u].COMPx = counterNow + irqPeriod;" in body
    assert "CMP[1u].COMPx = counterNow + fiqPeriod;" in body
    assert body.index("CMP[0u].COMPx = counterNow") < body.index("SETINTENA = 3u")


def test_idle_arms_alarm_starts_counter_then_enables_tms570_tick_irq():
    idle = _function_body("SC_Task_Idle")
    alarm = idle.index("SetRelAlarm(SC_ALARM_MAIN_ID, 1u, 1u)")
    counter = idle.index("rtiStartCounter();")
    irq = idle.index("Os_Port_Tms570_EnableRtiTick();")
    assert alarm < counter < irq
    assert "Os_Port_Tms570_TargetEnableRtiIrq();" in TMS_HW
    assert "vimChannelMap(2u, 2u" in TMS_TARGET
    assert "rtiREG1->SETINTENA = (uint32)1u;" in TMS_TARGET
    assert "(void)Os_BootstrapProcessCounterTick();" in TMS_TARGET


def test_tms570_periodic_termination_switchback_is_wired():
    service = TMS_TARGET[TMS_TARGET.index("void Os_Port_Tms570_RtiTickServiceCore"):]
    service = service[:service.index("\n}")]
    assert service.index("Os_PortEnterIsr2();") < service.index("Os_BootstrapProcessCounterTick();")
    assert service.index("Os_BootstrapProcessCounterTick();") < service.index("Os_PortExitIsr2();")
    assert "Os_Port_Tms570_HwSelectNextTask" not in service
    request = TMS_HW[TMS_HW.index("void Os_PortRequestContextSwitch"):]
    request = request[:request.index("\n}")]
    assert "Os_Port_Tms570_SwitchContextAsm(save, restore);" in request
    assert "Os_Port_Tms570_TargetMarkSwitchPending();" in request
    rebuild = TASK_BINDING[TASK_BINDING.index("StatusType Os_Port_RebuildTaskFrame"):]
    rebuild = rebuild[:rebuild.index("\n}")]
    assert "#elif defined(PLATFORM_TMS570)" in rebuild
    assert "Os_Port_Tms570_PrepareTaskContext(" in rebuild


def test_rti_handler_mode_switches_preserve_fiq_mask():
    # NMFI core: MSR can clear CPSR.F but never set it. VIM ch0/ch1 are
    # hardwired-FIQ, always enabled, and phantom-mapped; a dispatch-path
    # mode switch with F=0 (0x9F/0x92) permanently unmasks FIQ and a
    # latched ESM-high source becomes a silent take-return livelock.
    hardware = TMS_ASM.split("#else /* UNIT_TEST */")[0]
    assert "MOV     r2, #0xDF" in hardware
    assert "MOV     r2, #0xD2" in hardware
    assert "#0x9F" not in hardware
    assert "#0x92" not in hardware
    bringup = (ROOT / "firmware/platform/tms570/src/Os_Port_Tms570_Bringup.c").read_text(encoding="utf-8")
    assert '"MOV    r2, #0xDF' in bringup
    assert '"MOV    r2, #0xD2' in bringup
    assert "#0x9F" not in bringup
    assert "#0x92" not in bringup


def test_check_preemption_requires_valid_contexts():
    # A pending switch without a selected next task must never commit:
    # SwitchContextAsm would load SP/LR from flash bytes and BX into garbage.
    body = TMS_TARGET[TMS_TARGET.index("uint32 Os_Port_Tms570_CheckPreemption(void)"):]
    body = body[:body.index("\n}")]
    assert "os_tgt_next_task < TARGET_MAX_TASKS" in body
    assert "os_tgt_current_task < TARGET_MAX_TASKS" in body
    assert body.index("os_tgt_switch_pending = FALSE;") < body.index("os_tgt_next_task < TARGET_MAX_TASKS")


def test_dispatch_stack_monitor_uses_configured_stack_top():
    # os_dispatch_task runs in ISR context on ported hardware; recording
    # &stack_base_marker (an IRQ-stack address) as the safety task's stack
    # base makes the budget check measure garbage and latch a false
    # StackViolation at the task's first OS_STACK_SAMPLE.
    body = SCHEDULER[SCHEDULER.index("static void os_dispatch_task(TaskType NextTask)"):]
    body = body[:body.index("\nStatusType os_dispatch_one")]
    guarded = re.search(
        r"if \(\(Os_PortIsInIsrContext\(\) == TRUE\) &&\s*"
        r"\(os_task_stack_top_cfg\[NextTask\] != \(uintptr_t\)0u\)\) \{\s*"
        r"os_stack_monitor_enter_task\(NextTask, os_task_stack_top_cfg\[NextTask\]\);",
        body)
    assert guarded is not None
    # The hardware branch and the host-model branch each keep the marker
    # fallback; the ISR-context deferred dispatch is the only cfg-top user.
    assert body.count("os_stack_monitor_enter_task(NextTask, (uintptr_t)&stack_base_marker);") == 2


def test_bringup_tick_observability_is_isolated_and_change_driven():
    hw = TMS_HW
    target = TMS_TARGET
    assert "#ifdef OS_BOOTSTRAP_BRINGUP" in hw
    assert "[OSEK-TICK] enable returned" in MAIN
    for field in ("GCTRL=", "FRC0=", "CMP0=", "SETINTENA=", "INTFLAG=",
                  "REQMASK0=", "INTREQ0=", "FIRQPR0=", "IRQINDEX=", "CPSR=", "VEC2="):
        assert field in target
    assert "os_tgt_tick_entry_count++;" in target
    assert "[OSEK-KERNEL] isr=" in hw
    assert "os_counter_value" in hw
    assert "GetAlarm(SC_ALARM_MAIN_ID" in hw
    assert "(last_isr_count == 0u) && (isr_count > 0u)" in hw
    markers = (
        "counter started", "wrapper enter", "target enter",
        "before vim map", "after vim map", "before vim enable",
        "after vim enable", "before rti enable", "after rti enable",
        "before cpu irq enable", "after cpu irq enable", "wrapper exit",
        "enable returned",
    )
    combined = MAIN + hw + target
    for marker in markers:
        assert f"[OSEK-TICK] {marker}" in combined


def test_tms570_esm_high_fiq_is_fail_closed_and_never_acknowledges_group2():
    signature = 'void __attribute__((interrupt("FIQ"), noreturn)) Sc_Tms570_EsmHighInterrupt'
    handler = SC_TMS_HW[SC_TMS_HW.index(signature) :]
    handler = handler[: handler.index("\n}")]
    assert '__attribute__((interrupt("FIQ"), noreturn))' in handler
    relay_off = handler.index("GIO_DCLRA")
    sr2_capture = handler.index("sc_tms570_esm_high_sr2")
    ssr2_capture = handler.index("sc_tms570_esm_high_ssr2")
    park = handler.index("for (;;)")
    assert relay_off < sr2_capture < ssr2_capture < park
    assert "reg_write(ESM_BASE" not in handler
    vim_init = HAL_VIM[HAL_VIM.index("static const t_isrFuncPTR s_vim_init") :]
    vim_init = vim_init[: vim_init.index("&phantomInterrupt,        /* Channel 1")]
    assert "&Sc_Tms570_EsmHighInterrupt" in vim_init
    assert "vimChannelMap(0u, 0u" in SC_TMS_HW
    assert SC_TMS_HW.index("vimChannelMap(0u, 0u") < SC_TMS_HW.index('cpsie f')


def test_tms570_production_preserves_and_dumps_group2_status():
    # Group-2 status is retained diagnostic evidence. Production startup,
    # ESM init, and the VIM ch0 handler must observe it, never W1C it.
    assert "0xF51C" not in TMS_STARTUP
    assert "0xF53C" not in TMS_STARTUP

    esm_init = HAL_ESM[HAL_ESM.index("void esmInit(void)") :]
    esm_init = esm_init[: esm_init.index("\n}")]
    assert "esmREG->SR1[1U] =" not in esm_init
    assert "esmREG->SSR2" not in esm_init
    assert "esmREG->SR1[1U] =" not in HAL_ESM
    assert "esmREG->SR1[1U] =" not in HAL_VIM
    assert re.search(r"esmREG->SSR2\s*=", HAL_ESM) is None

    g3 = SC_TMS_HW[SC_TMS_HW.index("void esmGroup3Notification") :]
    g3 = g3[: g3.index("\n}")]
    assert "reg_write(ESM_BASE, ESM_SR1_1" not in g3
    for marker in ("ESM_SR2=", "ESM_SSR2="):
        assert marker in SC_TMS_HW


def test_tms570_bringup_recovery_precedes_one_way_fiq_unmask():
    init = SC_ESM[SC_ESM.index("void SC_ESM_Init(void)") :]
    init = init[: init.index("\n}")]
    assert "#ifdef OS_BOOTSTRAP_BRINGUP" in init
    assert "Os_Port_Tms570_BringupClearRetainedEsmGroup2();" in init
    assert init.index("Os_Port_Tms570_BringupClearRetainedEsmGroup2();") < init.index(
        "esm_install_high_level_handler();"
    )

    recovery = TMS_TARGET[
        TMS_TARGET.index("void Os_Port_Tms570_BringupClearRetainedEsmGroup2(void)") :
    ]
    recovery = recovery[: recovery.index("\n}")]
    assert "[BRINGUP-ESM] retained SR2=" in recovery
    assert "esmREG->SR1[1u] = sr2;" in recovery
    assert "esmREG->SSR2 = ssr2;" in recovery
    assert "(esmREG->SR1[1u] | esmREG->SSR2) == 0u" in recovery
    assert recovery.index("esmREG->SSR2 = ssr2;") < recovery.index(
        "vimREG->INTREQ0 = 1u;"
    )
    for marker in ("post SR2=", "SSR2=", "IOFFHR=", "INTREQ0="):
        assert marker in recovery
