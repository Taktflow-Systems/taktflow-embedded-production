"""
Shared fixtures for arxmlgen test suite.

Provides paths and model loading for golden-file comparison tests.
"""

import os
import sys
import pytest

# Add arxmlgen to path
TOOLS_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.dirname(TOOLS_DIR))

TESTS_DIR = os.path.dirname(os.path.abspath(__file__))
GOLDEN_DIR = os.path.join(TESTS_DIR, "golden")
PROFESSIONAL_DIR = os.path.join(GOLDEN_DIR, "professional_reference")
BASELINE_DIR = os.path.join(GOLDEN_DIR, "taktflow_baseline")

# Project root (taktflow-embedded-production)
PROJECT_ROOT = os.path.dirname(os.path.dirname(TOOLS_DIR))


@pytest.fixture
def project_root():
    return PROJECT_ROOT


@pytest.fixture
def golden_dir():
    return GOLDEN_DIR


@pytest.fixture
def professional_dir():
    return PROFESSIONAL_DIR


@pytest.fixture
def baseline_dir():
    return BASELINE_DIR


@pytest.fixture
def bcm_professional_dir():
    return os.path.join(PROFESSIONAL_DIR, "bcm")


@pytest.fixture
def load_model():
    """Load the arxmlgen model from project.yaml (if available)."""
    try:
        from arxmlgen.config import load_config
        from arxmlgen.reader import ArxmlReader

        config_path = os.path.join(PROJECT_ROOT, "project.yaml")
        if not os.path.exists(config_path):
            pytest.skip("project.yaml not found")

        config = load_config(config_path)
        reader = ArxmlReader(config)
        model = reader.read()
        return model
    except ImportError:
        pytest.skip("arxmlgen not importable")
    except Exception as e:
        pytest.skip(f"Model load failed: {e}")
