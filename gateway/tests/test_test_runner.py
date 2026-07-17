"""Regression tests for dashboard SIL test-run lifecycle reporting."""

from unittest.mock import MagicMock

from fault_inject.test_runner import DashboardTestRunner, TEST_SPECS


def make_runner() -> DashboardTestRunner:
    mqtt = MagicMock()
    return DashboardTestRunner(mqtt, MagicMock(), MagicMock())


def test_stop_while_waiting_for_run_is_aborted_not_failed(monkeypatch):
    runner = make_runner()

    def interrupted_wait(_run_id):
        runner._stop_requested = True
        return False

    monkeypatch.setattr(runner, "_wait_for_run", interrupted_wait)
    runner._run_suite([TEST_SPECS[0]], "stop-wait")

    assert runner.last_result["state"] == "aborted"
    assert runner.last_result["current_phase"] == "aborted"
    assert runner.last_result["current_index"] == 0
    assert runner.last_result["results"] == []
    assert runner.last_result["summary"]["failed"] == 0
    assert runner.status == "aborted"


def test_unavailable_run_state_remains_a_real_failure(monkeypatch):
    runner = make_runner()
    monkeypatch.setattr(runner, "_wait_for_run", lambda _run_id: False)
    runner._run_suite([TEST_SPECS[0]], "run-timeout")

    assert runner.last_result["state"] == "complete"
    assert runner.last_result["current_index"] == 1
    assert runner.last_result["summary"]["failed"] == 1
    assert runner.last_result["results"][0]["passed"] is False
    assert runner.status == "complete"


def test_runner_exception_is_not_reported_as_complete():
    runner = make_runner()
    runner._reset.side_effect = RuntimeError("reset failed")
    runner._run_suite([TEST_SPECS[0]], "run-error")

    assert runner.last_result["state"] == "error"
    assert runner.last_result["current_phase"] == "error"
    assert runner.last_result["summary"]["failed"] == 0
    assert runner.status == "error"
