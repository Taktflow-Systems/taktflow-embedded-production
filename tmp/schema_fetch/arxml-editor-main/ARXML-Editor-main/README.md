

# ARXML Editor (Qt6 / C++)

A lightweight, cross-platform AUTOSAR ARXML Editor built with Qt 6 and C++.  
It supports XML schema validation and provides an intuitive tree-based view for exploring and editing AUTOSAR XML files.

---

## Overview

The ARXML Editor is designed for engineers who work with AUTOSAR configuration files and need a fast, portable, and developer-friendly way to inspect and validate `.arxml` files.  
It provides essential editing and validation features while maintaining a simple and responsive interface.

---

## Key Features

- Hierarchical Tree View — Displays the full ARXML structure in a collapsible tree.
- Property Table — Edit element attributes and values directly.
- Schema Validation — Validates changes against AUTOSAR `.xsd` schemas.
- Search and Filter — Quickly locate elements in large ARXML files.
- Save and Export — Supports standard save and save-as functionality.
- Cross-Platform — Runs on Windows and Linux using the same source code.

---

## Architecture

| Layer | Description |
|-------|--------------|
| MainWindow | User interface layer managing the tree view, property table, and user interactions. |
| ArxmlModel | Handles XML parsing, DOM manipulation, and save/load operations. |
| ArxmlValidator | Performs schema validation using the AUTOSAR XSD file. |

---

## Screenshots

Tool UI:
<img width="901" height="631" alt="image" src="https://github.com/user-attachments/assets/6dec1772-111c-452c-8ae3-5dd85b987e15" />

When the validate button is pressed, the user is asked to provide the .xsd file to be checked against.
<img width="1047" height="689" alt="image" src="https://github.com/user-attachments/assets/ef1b838f-b10c-41cb-a77a-0f4de07b3471" />

Adding a child node.
<img width="1421" height="438" alt="image" src="https://github.com/user-attachments/assets/09f1fc28-fc5c-451d-b2cd-7d05b36f75c0" />














