// main_window.cpp
//
// Main window implementation with multi-panel layout

#include "main_window.hpp"
#include "arxml_model.hpp"
#include "arxml_validator.hpp"

#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QTableWidget>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextEdit>
#include <QHeaderView>
#include <QMenu>
#include <QInputDialog>
#include <QDateTime>
#include <QLabel>
#include <QGridLayout>
#include <QVariant>
#include <QList>
#include <QMap>
#include <QTabWidget>
#include <QGroupBox>
#include <QLineEdit>
#include <QFormLayout>
#include <QScrollArea>
#include <QRadioButton>
#include <QButtonGroup>
#include <QAbstractButton>
#include <QCheckBox>
#include <QComboBox>
#include <QIntValidator>
#include <QIcon>
#include <QListWidget>
#include <QSplitter>
#include <QMouseEvent>
#include <QEvent>
#include <QTemporaryFile>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_treeWidget(new QTreeWidget),
      m_propertyTabWidget(new QTabWidget),
      m_propertyTable(new QTableWidget),
      m_portsPropertiesTab(new QWidget),
      m_portsApiOptionsTab(new QWidget),
      m_portsCommSpecTab(new QWidget),
      m_portsDescriptionTab(new QTextEdit),
      m_logTabWidget(new QTabWidget),
      m_portNameEdit(new QLineEdit),
      m_portInterfaceGroup(new QGroupBox),
      m_portInterfaceNameEdit(new QLineEdit),
      m_blueprintGroup(new QGroupBox),
      m_blueprintNameEdit(new QLineEdit),
      m_directionGroup(new QGroupBox),
      m_directionButtonGroup(new QButtonGroup),
      m_directionRadio1(new QRadioButton),
      m_directionRadio2(new QRadioButton),
      m_directionRadio3(new QRadioButton),
      m_apiOptionsGroup(new QGroupBox),
      m_enableIndirectApiCheck(new QCheckBox),
      m_enableApiUsageByAddressCheck(new QCheckBox),
      m_transformationErrorHandlingCheck(new QCheckBox),
      m_portDefinedArgsGroup(new QGroupBox),
      m_portDefinedArgsTable(new QTableWidget),
      m_editArgsButton(new QPushButton),
      m_getFromButton(new QPushButton),
      m_copyToButton(new QPushButton),
      m_commSpecDeElementsList(new QListWidget),
      m_commSpecSubTabs(new QTabWidget),
      m_senderComSpecTab(new QWidget),
      m_receiverComSpecTab(new QWidget),
      m_clientServerComSpecTab(new QWidget),
      m_serverComSpecTab(new QWidget),
      m_interfacePropertiesTab(new QWidget),
      m_commSpecDescriptionTab(new QWidget),
      m_commSpecNoPropertiesLabel(new QLabel),
      m_receiverInitValueEdit(new QLineEdit),
      m_receiverInitValueTypeCombo(new QComboBox),
      m_receiverInitValueEditButton(new QPushButton),
      m_rxFilterButtonGroup(new QButtonGroup),
      m_rxFilterAlwaysRadio(new QRadioButton),
      m_rxFilterMaskedRadio(new QRadioButton),
      m_receiverUsesEndToEndProtectionCheck(new QCheckBox),
      m_handleNeverReceivedCheck(new QCheckBox),
      m_enableUpdateCheck(new QCheckBox),
      m_aliveTimeoutEdit(new QLineEdit),
      m_queueLengthEdit(new QLineEdit),
      m_transformationGroup(new QGroupBox),
      m_transformationTypeCombo(new QComboBox),
      m_transformationDisabledCheck(new QCheckBox),
      m_maxDeltaCounterEdit(new QLineEdit),
      m_maxErrorStateInitEdit(new QLineEdit),
      m_maxErrorStateInvalidEdit(new QLineEdit),
      m_maxErrorStateValidEdit(new QLineEdit),
      m_minOkStateInitEdit(new QLineEdit),
      m_minOkStateInvalidEdit(new QLineEdit),
      m_minOkStateValidEdit(new QLineEdit),
      m_syncCounterInitEdit(new QLineEdit),
      m_clientServerDeElementsList(new QListWidget),
      m_clientServerTransformationGroup(new QGroupBox),
      m_clientServerTransformationTypeCombo(new QComboBox),
      m_clientServerTransformationDisabledCheck(new QCheckBox),
      m_clientServerMaxDeltaCounterEdit(new QLineEdit),
      m_clientServerMaxErrorStateInitEdit(new QLineEdit),
      m_clientServerMaxErrorStateInvalidEdit(new QLineEdit),
      m_clientServerMaxErrorStateValidEdit(new QLineEdit),
      m_clientServerMaxNoNewOrRepeatedDataEdit(new QLineEdit),
      m_clientServerMinOkStateInitEdit(new QLineEdit),
      m_clientServerMinOkStateInvalidEdit(new QLineEdit),
      m_clientServerMinOkStateValidEdit(new QLineEdit),
      m_clientServerSyncCounterInitEdit(new QLineEdit),
      m_clientServerWindowSizeEdit(new QLineEdit),
      m_clientServerNoPropertiesLabel(new QLabel),
      m_serverQueueLengthEdit(new QLineEdit),
      m_interfacePropNameEdit(new QLineEdit),
      m_dataTypeGroup(new QGroupBox),
      m_dataTypeEdit(new QLineEdit),
      m_dataTypeButton1(new QPushButton),
      m_dataTypeButton2(new QPushButton),
      m_dataTypeButton3(new QPushButton),
      m_dataConstraintEdit(new QLineEdit),
      m_dataConstraintButton(new QPushButton),
      m_useQueuedCommCheck(new QCheckBox),
      m_measurementCalibrationGroup(new QGroupBox),
      m_calibrationAccessCombo(new QComboBox),
      m_handleInvalidGroup(new QGroupBox),
      m_handleInvalidButtonGroup(new QButtonGroup),
      m_handleInvalidKeepRadio(new QRadioButton),
      m_handleInvalidReplaceRadio(new QRadioButton),
      m_handleInvalidNoneRadio(new QRadioButton),
      m_initValueEdit(new QLineEdit),
      m_initValueTypeCombo(new QComboBox),
      m_initValueEditButton(new QPushButton),
      m_usesTxAcknowledgeCheck(new QCheckBox),
      m_timeoutEdit(new QLineEdit),
      m_timeoutUnitCombo(new QComboBox),
      m_usesEndToEndProtectionCheck(new QCheckBox),
      m_actionLog(new QTextEdit),
      m_openButton(new QPushButton(tr("Open"))),
      m_saveButton(new QPushButton(tr("Save"))),
      m_saveAsButton(new QPushButton(tr("Save As"))),
      m_validateButton(new QPushButton(tr("Validate"))),
      m_searchBox(new QLineEdit),
      m_model(new ArxmlModel),
      m_validator(new ArxmlValidator)
{
    // Central widget and layout
    QWidget *central = new QWidget(this);
    setCentralWidget(central);
    QVBoxLayout *mainLayout = new QVBoxLayout(central);

    // Top toolbar
    QHBoxLayout *toolbarLayout = new QHBoxLayout;
    toolbarLayout->addWidget(m_openButton);
    toolbarLayout->addWidget(m_saveButton);
    toolbarLayout->addWidget(m_saveAsButton);
    toolbarLayout->addWidget(m_validateButton);
    toolbarLayout->addSpacing(10);
    QLabel *searchLabel = new QLabel(tr("Search:"));
    toolbarLayout->addWidget(searchLabel);
    m_searchBox->setPlaceholderText(tr("Filter tree..."));
    m_searchBox->setMaximumWidth(200);
    toolbarLayout->addWidget(m_searchBox);
    toolbarLayout->addStretch();
    mainLayout->addLayout(toolbarLayout);

    // Main content area - horizontal splitter for tree and properties
    QSplitter *mainSplitter = new QSplitter(Qt::Horizontal);
    
    // Left panel - Tree view
    QWidget *leftPanel = new QWidget;
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    m_treeWidget->setHeaderLabels(QStringList() << tr("Name") << tr("Package"));
    m_treeWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_treeWidget->header()->setDefaultSectionSize(200);
    leftLayout->addWidget(m_treeWidget);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    mainSplitter->addWidget(leftPanel);

    // Right panel - Properties
    QWidget *propPanel = new QWidget;
    QVBoxLayout *propLayout = new QVBoxLayout(propPanel);
    
    // Setup standard property table
    m_propertyTable->setColumnCount(2);
    m_propertyTable->setHorizontalHeaderLabels(QStringList() << tr("Property") << tr("Value"));
    m_propertyTable->horizontalHeader()->setStretchLastSection(true);
    m_propertyTable->verticalHeader()->setVisible(false);
    m_propertyTable->setEditTriggers(QAbstractItemView::AllEditTriggers);
    
    // Setup PORTS tabs
    setupPortsTabs();
    
    // Add both widgets to layout, but show only the standard table initially
    propLayout->addWidget(m_propertyTable);
    propLayout->addWidget(m_propertyTabWidget);
    m_propertyTabWidget->setVisible(false);  // Hide tabs initially
    propLayout->setContentsMargins(0, 0, 0, 0);

    mainSplitter->addWidget(propPanel);

    // Set horizontal splitter sizes to 2/3 and 1/3
    mainSplitter->setStretchFactor(0, 2);
    mainSplitter->setStretchFactor(1, 1);
    mainLayout->addWidget(mainSplitter, 1);  // Give main content stretch factor 1

    // Bottom - Action Log and Messages tabs (spans full width, small height)
    QWidget *logPanel = new QWidget;
    QVBoxLayout *logLayout = new QVBoxLayout(logPanel);
    
    // Setup action log
    m_actionLog->setReadOnly(true);
    
    // Setup messages tab (placeholder for now)
    m_messagesTab = new QWidget;
    QVBoxLayout *messagesLayout = new QVBoxLayout(m_messagesTab);
    QLabel *messagesLabel = new QLabel(tr("Messages tab - functionality to be added"));
    messagesLabel->setAlignment(Qt::AlignCenter);
    messagesLayout->addWidget(messagesLabel);
    
    // Add tabs to tab widget
    m_logTabWidget->addTab(m_actionLog, tr("Action Log"));
    m_logTabWidget->addTab(m_messagesTab, tr("Messages"));
    m_logTabWidget->setFixedHeight(120);  // Fixed height for the tab widget
    
    logLayout->addWidget(m_logTabWidget);
    logLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(logPanel, 0);  // No stretch for log tabs

    // Initially disable Save, Save As, and Validate buttons (no file opened yet)
    m_saveButton->setEnabled(false);
    m_saveAsButton->setEnabled(false);
    m_validateButton->setEnabled(false);
    
    // Connect signals
    connect(m_openButton, &QPushButton::clicked, this, &MainWindow::openFile);
    connect(m_saveButton, &QPushButton::clicked, this, &MainWindow::saveFile);
    connect(m_saveAsButton, &QPushButton::clicked, this, &MainWindow::saveFileAs);
    connect(m_validateButton, &QPushButton::clicked, this, &MainWindow::validateDocument);
    connect(m_treeWidget, &QTreeWidget::currentItemChanged,
            this, &MainWindow::onCurrentItemChanged);
    connect(m_propertyTable, &QTableWidget::itemChanged,
            this, &MainWindow::onPropertyItemChanged);
    connect(m_portNameEdit, &QLineEdit::textChanged,
            this, &MainWindow::onPortPropertyChanged);
    connect(m_directionButtonGroup, &QButtonGroup::buttonClicked,
            this, [this](QAbstractButton* button) {
                int id = m_directionButtonGroup->id(button);
                onDirectionChanged(id);
            });
    // Port API Options tab uses checkboxes and buttons, no table item changes to connect
    // Port Communication Spec tab is now a form widget, no table item changes to connect
    connect(m_portsDescriptionTab, &QTextEdit::textChanged,
            this, &MainWindow::onDescriptionTextChanged);
    connect(m_commSpecDeElementsList, &QListWidget::itemSelectionChanged,
            this, &MainWindow::onCommSpecDeElementSelected);
    connect(m_commSpecDeElementsList, &QListWidget::currentItemChanged,
            this, &MainWindow::onCommSpecDeElementSelected);
    connect(m_searchBox, &QLineEdit::textChanged,
            this, &MainWindow::onSearchTextChanged);
    
    // Context menu
    m_treeWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_treeWidget, &QTreeWidget::customContextMenuRequested,
            this, &MainWindow::showContextMenu);

    // Window settings
    setWindowTitle(tr("ARXML Editor"));
    resize(1200, 800);
}

MainWindow::~MainWindow()
{
}

void MainWindow::openFile()
{
    QString fileName = QFileDialog::getOpenFileName(this,
                                                    tr("Open ARXML File"),
                                                    "",
                                                    tr("ARXML Files (*.arxml *.xml);;All Files (*)"));
    if (fileName.isEmpty())
        return;

    if (m_model->loadFromFile(fileName)) {
        m_currentFileName = fileName;
        logAction(tr("Opened file: %1").arg(fileName));
        
        // Rebuild tree
        m_treeWidget->clear();
        if (m_model->rootElement()) {
            buildTreeRecursive(m_model->rootElement(), nullptr);
            // Collapse all items initially
            m_treeWidget->collapseAll();
            // Select first item if available
            if (m_treeWidget->topLevelItemCount() > 0) {
                m_treeWidget->setCurrentItem(m_treeWidget->topLevelItem(0));
            }
        }
        
        // Enable Save, Save As, and Validate buttons
        m_saveButton->setEnabled(true);
        m_saveAsButton->setEnabled(true);
        m_validateButton->setEnabled(true);
    } else {
        QMessageBox::critical(this, tr("Error"),
                              tr("Failed to open file: %1").arg(fileName));
    }
}

void MainWindow::buildTreeRecursive(const std::shared_ptr<ArxmlElement>& elem, QTreeWidgetItem* parentItem)
{
    QTreeWidgetItem* item;
    if (parentItem) {
        item = new QTreeWidgetItem(parentItem);
    } else {
        item = new QTreeWidgetItem(m_treeWidget);
    }

    // Get display info (name and package)
    QString name, package;
    getElementDisplayInfo(elem.get(), name, package);
    
    item->setText(0, name);
    item->setText(1, package);
    
    // Store element index path instead of raw pointer
    QList<int> indexPath = m_model->getElementIndexPath(elem.get());
    item->setData(0, Qt::UserRole, QVariant::fromValue(indexPath));

    // Build children
    for (const auto& child : elem->children) {
        buildTreeRecursive(child, item);
    }
}

void MainWindow::getElementDisplayInfo(ArxmlElement* elem, QString& name, QString& package) const
{
    if (!elem) {
        name = "";
        package = "";
        return;
    }
    
    name = elem->tagName;
    package = "";
    
    // Try to find SHORT-NAME or similar in attributes/children
    for (const auto& attr : elem->attributes) {
        if (attr.first.contains("XMLNS", Qt::CaseInsensitive)) {
            package = attr.second;
            break;
        }
    }
    
    // Look for SHORT-NAME or name-like children
    for (const auto& child : elem->children) {
        if (child->tagName.contains("SHORT-NAME", Qt::CaseInsensitive) && 
            !child->text.isEmpty()) {
            name = child->text;
        } else if (child->tagName.contains("NAME", Qt::CaseInsensitive) && 
                   !child->text.isEmpty() && name == elem->tagName) {
            name = child->text;
        } else if (child->tagName.contains("PACKAGE", Qt::CaseInsensitive) &&
                   !child->text.isEmpty()) {
            package = child->text;
        }
    }
}

void MainWindow::refreshTreeItem(QTreeWidgetItem* item, ArxmlElement* elem)
{
    if (!item || !elem) {
        return;
    }
    
    QString name, package;
    getElementDisplayInfo(elem, name, package);
    
    item->setText(0, name);
    item->setText(1, package);
}

void MainWindow::saveFile()
{
    if (!m_model->rootElement()) {
        QMessageBox::information(this, tr("No Document"),
                                tr("Please open an ARXML file before saving."));
        return;
    }

    if (m_currentFileName.isEmpty()) {
        saveFileAs();
        return;
    }

    if (m_model->saveToFile(m_currentFileName)) {
        logAction(tr("Saved file: %1").arg(m_currentFileName));
        QMessageBox::information(this, tr("Saved"),
                                tr("File saved to %1").arg(m_currentFileName));
    }
}

void MainWindow::saveFileAs()
{
    if (!m_model->rootElement()) {
        QMessageBox::information(this, tr("No Document"),
                                tr("Please open an ARXML file before saving."));
        return;
    }

    QString fileName = QFileDialog::getSaveFileName(this,
                                                     tr("Save ARXML File"),
                                                     "",
                                                     tr("ARXML Files (*.arxml *.xml);;All Files (*)"));
    if (fileName.isEmpty())
        return;

    if (m_model->saveToFile(fileName)) {
        m_currentFileName = fileName;
        logAction(tr("Saved file as: %1").arg(fileName));
        QMessageBox::information(this, tr("Saved"),
                                tr("File saved to %1").arg(fileName));
    } else {
        QMessageBox::critical(this, tr("Error"),
                              tr("Failed to save file: %1").arg(fileName));
    }
}

void MainWindow::onCurrentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous)
{
    Q_UNUSED(previous);
    
    // Clear all fields
    m_propertyTable->clearContents();
    m_propertyTable->setRowCount(0);
    m_portNameEdit->blockSignals(true);
    m_portNameEdit->clear();
    m_portInterfaceNameEdit->clear();
    m_blueprintNameEdit->setText(tr("<None>"));
    m_directionRadio1->setChecked(false);
    m_directionRadio2->setChecked(false);
    m_directionRadio3->setChecked(false);
    m_enableIndirectApiCheck->setChecked(false);
    m_enableApiUsageByAddressCheck->setChecked(false);
    m_transformationErrorHandlingCheck->setChecked(false);
    m_portDefinedArgsTable->clearContents();
    m_portDefinedArgsTable->setRowCount(0);
    m_commSpecDeElementsList->clear();
    m_dataElementToComSpec.clear();
    m_initValueEdit->clear();
    m_initValueTypeCombo->setCurrentIndex(2); // Reset to "Array"
    m_usesTxAcknowledgeCheck->setChecked(false);
    m_timeoutEdit->clear();
    m_timeoutUnitCombo->setCurrentIndex(0);
    m_usesEndToEndProtectionCheck->setChecked(false);
    m_portsDescriptionTab->clear();

    // Unblock signals before populating
    m_portNameEdit->blockSignals(false);

    if (!current)
        return;

    auto elem = getElementForItem(current);
    if (!elem)
        return;

    bool isPorts = isPortsElement(elem.get());
    
    if (isPorts) {
        // Show PORTS-specific tabs
        m_propertyTable->setVisible(false);
        m_propertyTabWidget->setVisible(true);
        m_propertyTabWidget->addTab(m_portsPropertiesTab, tr("Properties"));
        m_propertyTabWidget->addTab(m_portsApiOptionsTab, tr("Port API Options"));
        m_propertyTabWidget->addTab(m_portsCommSpecTab, tr("Communication Spec"));
        m_propertyTabWidget->addTab(m_portsDescriptionTab, tr("Description"));
        
        // Populate PORTS-specific tabs
        populatePortsTabs(elem.get());
    } else {
        // Hide tabs and show standard property table
        m_propertyTabWidget->setVisible(false);
        m_propertyTable->setVisible(true);
        populateStandardPropertyTable(elem.get());
    }
}

void MainWindow::populateStandardPropertyTable(ArxmlElement* elem)
{
    if (!elem)
        return;

    m_propertyTable->blockSignals(true);
    m_propertyTable->clearContents();
    m_propertyTable->setRowCount(0);

    int row = 0;

    // Attributes
    for (const auto& attr : elem->attributes) {
        m_propertyTable->insertRow(row);
        QTableWidgetItem *attrName = new QTableWidgetItem(attr.first);
        attrName->setFlags(attrName->flags() & ~Qt::ItemIsEditable);
        m_propertyTable->setItem(row, 0, attrName);
        QTableWidgetItem *attrValue = new QTableWidgetItem(attr.second);
        m_propertyTable->setItem(row, 1, attrValue);
        row++;
    }

    // Text content
    if (!elem->text.isEmpty()) {
        m_propertyTable->insertRow(row);
        QTableWidgetItem *textName = new QTableWidgetItem(tr("Text"));
        textName->setFlags(textName->flags() & ~Qt::ItemIsEditable);
        m_propertyTable->setItem(row, 0, textName);
        QTableWidgetItem *textValue = new QTableWidgetItem(elem->text);
        m_propertyTable->setItem(row, 1, textValue);
    }
    
    m_propertyTable->blockSignals(false);
}

void MainWindow::setupPortsTabs()
{
    // Setup Properties tab as a form widget
    QVBoxLayout *propertiesLayout = new QVBoxLayout(m_portsPropertiesTab);
    propertiesLayout->setContentsMargins(10, 10, 10, 10);
    propertiesLayout->setSpacing(15);
    
    // Name field (editable)
    QFormLayout *nameLayout = new QFormLayout;
    nameLayout->addRow(tr("Name:"), m_portNameEdit);
    propertiesLayout->addLayout(nameLayout);
    
    // Port Interface group box
    m_portInterfaceGroup->setTitle(tr("Port Interface"));
    m_portInterfaceGroup->setStyleSheet("QGroupBox { border: 1px solid #ccc; border-radius: 3px; margin-top: 10px; padding-top: 10px; } QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; }");
    QFormLayout *interfaceLayout = new QFormLayout(m_portInterfaceGroup);
    interfaceLayout->addRow(tr("Name:"), m_portInterfaceNameEdit);
    m_portInterfaceNameEdit->setReadOnly(true); // Non-editable
    propertiesLayout->addWidget(m_portInterfaceGroup);
    
    // Blueprint group box
    m_blueprintGroup->setTitle(tr("Blueprint"));
    m_blueprintGroup->setStyleSheet("QGroupBox { border: 1px solid #ccc; border-radius: 3px; margin-top: 10px; padding-top: 10px; } QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; }");
    QFormLayout *blueprintLayout = new QFormLayout(m_blueprintGroup);
    blueprintLayout->addRow(tr("Name:"), m_blueprintNameEdit);
    m_blueprintNameEdit->setText(tr("<None>"));
    m_blueprintNameEdit->setReadOnly(true); // Non-editable
    propertiesLayout->addWidget(m_blueprintGroup);
    
    // Direction group box with radio buttons
    m_directionGroup->setTitle(tr("Direction"));
    m_directionGroup->setStyleSheet("QGroupBox { border: 1px solid #ccc; border-radius: 3px; margin-top: 10px; padding-top: 10px; } QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; }");
    QVBoxLayout *directionLayout = new QVBoxLayout(m_directionGroup);
    m_directionButtonGroup->addButton(m_directionRadio1, 0);
    m_directionButtonGroup->addButton(m_directionRadio2, 1);
    m_directionButtonGroup->addButton(m_directionRadio3, 2);
    directionLayout->addWidget(m_directionRadio1);
    directionLayout->addWidget(m_directionRadio2);
    directionLayout->addWidget(m_directionRadio3);
    directionLayout->addStretch();
    propertiesLayout->addWidget(m_directionGroup);
    
    propertiesLayout->addStretch();

    // Setup Port API Options tab
    QVBoxLayout *apiOptionsLayout = new QVBoxLayout(m_portsApiOptionsTab);
    apiOptionsLayout->setContentsMargins(10, 10, 10, 10);
    apiOptionsLayout->setSpacing(15);
    
    // API Options group box
    m_apiOptionsGroup->setTitle(tr("API Options"));
    m_apiOptionsGroup->setStyleSheet("QGroupBox { border: 1px solid #ccc; border-radius: 3px; margin-top: 10px; padding-top: 10px; } QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; }");
    QVBoxLayout *apiOptionsGroupLayout = new QVBoxLayout(m_apiOptionsGroup);
    m_enableIndirectApiCheck->setText(tr("Enable indirect API"));
    m_enableApiUsageByAddressCheck->setText(tr("Enable API usage by address"));
    m_transformationErrorHandlingCheck->setText(tr("Transformation Error Handling"));
    apiOptionsGroupLayout->addWidget(m_enableIndirectApiCheck);
    apiOptionsGroupLayout->addWidget(m_enableApiUsageByAddressCheck);
    apiOptionsGroupLayout->addWidget(m_transformationErrorHandlingCheck);
    apiOptionsLayout->addWidget(m_apiOptionsGroup);
    
    // Port Defined Argument Values group box
    m_portDefinedArgsGroup->setTitle(tr("Port Defined Argument Values"));
    m_portDefinedArgsGroup->setStyleSheet("QGroupBox { border: 1px solid #ccc; border-radius: 3px; margin-top: 10px; padding-top: 10px; } QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; }");
    QHBoxLayout *argsGroupLayout = new QHBoxLayout(m_portDefinedArgsGroup);
    
    // Table for arguments
    m_portDefinedArgsTable->setColumnCount(4);
    m_portDefinedArgsTable->setHorizontalHeaderLabels(QStringList() << tr("Index") << tr("Name") << tr("Data Type") << tr("Value"));
    m_portDefinedArgsTable->horizontalHeader()->setStretchLastSection(true);
    m_portDefinedArgsTable->verticalHeader()->setVisible(false);
    m_portDefinedArgsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    argsGroupLayout->addWidget(m_portDefinedArgsTable);
    
    // Vertical buttons
    QVBoxLayout *argsButtonsLayout = new QVBoxLayout;
    m_editArgsButton->setText(tr("Edit..."));
    m_editArgsButton->setEnabled(false);
    m_getFromButton->setText(tr("Get from..."));
    m_getFromButton->setEnabled(false);
    m_copyToButton->setText(tr("Copy to..."));
    m_copyToButton->setEnabled(false);
    argsButtonsLayout->addWidget(m_editArgsButton);
    argsButtonsLayout->addWidget(m_getFromButton);
    argsButtonsLayout->addWidget(m_copyToButton);
    argsButtonsLayout->addStretch();
    argsGroupLayout->addLayout(argsButtonsLayout);
    
    apiOptionsLayout->addWidget(m_portDefinedArgsGroup);
    apiOptionsLayout->addStretch();

    // Setup Communication Spec tab
    QVBoxLayout *commSpecMainLayout = new QVBoxLayout(m_portsCommSpecTab);
    commSpecMainLayout->setContentsMargins(0, 0, 0, 0);
    commSpecMainLayout->setSpacing(0);
    
    QSplitter *commSpecSplitter = new QSplitter(Qt::Horizontal, m_portsCommSpecTab);
    
    // Left panel: De_... elements list
    QWidget *deElementsPanel = new QWidget;
    deElementsPanel->installEventFilter(this);  // Install event filter to handle clicks
    QVBoxLayout *deElementsLayout = new QVBoxLayout(deElementsPanel);
    deElementsLayout->setContentsMargins(5, 5, 5, 5);
    QLabel *deElementsLabel = new QLabel(tr("Data Elements:"));
    deElementsLayout->addWidget(deElementsLabel);
    m_commSpecDeElementsList->setSelectionMode(QAbstractItemView::SingleSelection);
    // Data elements will be populated from XML in populatePortsTabs
    m_commSpecDeElementsList->installEventFilter(this);  // Install event filter to handle clicks on empty space
    // Also install event filter on the viewport to catch clicks on empty space
    if (m_commSpecDeElementsList->viewport()) {
        m_commSpecDeElementsList->viewport()->installEventFilter(this);
    }
    deElementsLayout->addWidget(m_commSpecDeElementsList);
    commSpecSplitter->addWidget(deElementsPanel);
    
    // Right panel: Container widget for both sub-tabs and "No properties" message
    QWidget *rightPanelContainer = new QWidget;
    QVBoxLayout *rightPanelLayout = new QVBoxLayout(rightPanelContainer);
    rightPanelLayout->setContentsMargins(0, 0, 0, 0);
    
    // "No properties available" label (shown initially)
    m_commSpecNoPropertiesLabel->setText(tr("No properties available"));
    m_commSpecNoPropertiesLabel->setAlignment(Qt::AlignCenter);
    m_commSpecNoPropertiesLabel->setStyleSheet("QLabel { color: #888888; font-size: 14px; }");
    rightPanelLayout->addWidget(m_commSpecNoPropertiesLabel);
    
    // Setup sub-tabs (hidden initially, shown when De element is selected)
    // Sender ComSpec tab - contains the form fields
    QVBoxLayout *senderComSpecLayout = new QVBoxLayout(m_senderComSpecTab);
    senderComSpecLayout->setContentsMargins(10, 10, 10, 10);
    senderComSpecLayout->setSpacing(15);
    
    // Init Value field with type dropdown and edit button
    QHBoxLayout *initValueLayout = new QHBoxLayout;
    QLabel *initValueLabel = new QLabel(tr("Init Value:"));
    initValueLayout->addWidget(initValueLabel);
    initValueLayout->addWidget(m_initValueEdit);
    m_initValueTypeCombo->addItems(QStringList() << tr("Textual") << tr("Numeric") << tr("Array") 
                                                   << tr("Record") << tr("Reference") << tr("Application") << tr("None"));
    m_initValueTypeCombo->setCurrentIndex(1); // Default to "Numeric" (as shown in screenshot)
    initValueLayout->addWidget(m_initValueTypeCombo);
    m_initValueEditButton->setIcon(QIcon::fromTheme("document-edit"));
    m_initValueEditButton->setToolTip(tr("Edit Init Value"));
    initValueLayout->addWidget(m_initValueEditButton);
    senderComSpecLayout->addLayout(initValueLayout);
    
    // Uses Tx Acknowledge checkbox
    m_usesTxAcknowledgeCheck->setText(tr("Uses Tx Acknowledge"));
    senderComSpecLayout->addWidget(m_usesTxAcknowledgeCheck);
    
    // Timeout field with unit dropdown
    QHBoxLayout *timeoutLayout = new QHBoxLayout;
    QLabel *timeoutLabel = new QLabel(tr("Timeout:"));
    timeoutLayout->addWidget(timeoutLabel);
    m_timeoutEdit->setValidator(new QIntValidator(0, 999999, m_timeoutEdit));
    timeoutLayout->addWidget(m_timeoutEdit);
    m_timeoutUnitCombo->addItem(tr("sec"));
    m_timeoutUnitCombo->setFixedWidth(60);
    timeoutLayout->addWidget(m_timeoutUnitCombo);
    timeoutLayout->addStretch();
    senderComSpecLayout->addLayout(timeoutLayout);
    
    // Uses End-to-End Protection checkbox
    m_usesEndToEndProtectionCheck->setText(tr("Uses End-to-End Protection"));
    senderComSpecLayout->addWidget(m_usesEndToEndProtectionCheck);
    
    senderComSpecLayout->addStretch();
    
    // Receiver ComSpec tab - for R-PORT-PROTOTYPE with SENDER-RECEIVER-INTERFACE
    QVBoxLayout *receiverComSpecLayout = new QVBoxLayout(m_receiverComSpecTab);
    receiverComSpecLayout->setContentsMargins(10, 10, 10, 10);
    receiverComSpecLayout->setSpacing(15);
    
    // Init Value field
    QHBoxLayout *receiverInitValueLayout = new QHBoxLayout;
    QLabel *receiverInitValueLabel = new QLabel(tr("Init Value:"));
    receiverInitValueLayout->addWidget(receiverInitValueLabel);
    m_receiverInitValueEdit->setText("0");
    receiverInitValueLayout->addWidget(m_receiverInitValueEdit);
    m_receiverInitValueTypeCombo->addItems(QStringList() << tr("Textual") << tr("Numeric") << tr("Array") 
                                                           << tr("Record") << tr("Reference") << tr("Application") << tr("None"));
    m_receiverInitValueTypeCombo->setCurrentIndex(1); // Default to "Numeric"
    receiverInitValueLayout->addWidget(m_receiverInitValueTypeCombo);
    m_receiverInitValueEditButton->setIcon(QIcon::fromTheme("document-open"));
    m_receiverInitValueEditButton->setFixedSize(24, 24);
    m_receiverInitValueEditButton->setToolTip(tr("Edit Init Value"));
    receiverInitValueLayout->addWidget(m_receiverInitValueEditButton);
    receiverComSpecLayout->addLayout(receiverInitValueLayout);
    
    // Rx Filter group box with radio buttons
    QGroupBox *rxFilterGroup = new QGroupBox(tr("Rx Filter"));
    rxFilterGroup->setStyleSheet("QGroupBox { border: 1px solid #ccc; border-radius: 3px; margin-top: 10px; padding-top: 10px; } QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; }");
    QVBoxLayout *rxFilterLayout = new QVBoxLayout(rxFilterGroup);
    m_rxFilterAlwaysRadio->setText(tr("Always"));
    m_rxFilterAlwaysRadio->setChecked(true);  // Default checked
    m_rxFilterMaskedRadio->setText(tr("MaskedNewDiffersMaskedOld"));
    m_rxFilterButtonGroup->addButton(m_rxFilterAlwaysRadio, 0);
    m_rxFilterButtonGroup->addButton(m_rxFilterMaskedRadio, 1);
    rxFilterLayout->addWidget(m_rxFilterAlwaysRadio);
    rxFilterLayout->addWidget(m_rxFilterMaskedRadio);
    rxFilterLayout->addStretch();
    receiverComSpecLayout->addWidget(rxFilterGroup);
    
    // Checkboxes
    m_receiverUsesEndToEndProtectionCheck->setText(tr("Uses End-to-End Protection"));
    receiverComSpecLayout->addWidget(m_receiverUsesEndToEndProtectionCheck);
    m_handleNeverReceivedCheck->setText(tr("Handle Never Received"));
    receiverComSpecLayout->addWidget(m_handleNeverReceivedCheck);
    m_enableUpdateCheck->setText(tr("Enable Update"));
    receiverComSpecLayout->addWidget(m_enableUpdateCheck);
    
    // Alive Timeout [s]
    QHBoxLayout *aliveTimeoutLayout = new QHBoxLayout;
    QLabel *aliveTimeoutLabel = new QLabel(tr("Alive Timeout [s]:"));
    aliveTimeoutLayout->addWidget(aliveTimeoutLabel);
    m_aliveTimeoutEdit->setText("0");
    m_aliveTimeoutEdit->setValidator(new QIntValidator(0, 999999, m_aliveTimeoutEdit));
    aliveTimeoutLayout->addWidget(m_aliveTimeoutEdit);
    aliveTimeoutLayout->addStretch();
    receiverComSpecLayout->addLayout(aliveTimeoutLayout);
    
    // Queue Length
    QHBoxLayout *queueLengthLayout = new QHBoxLayout;
    QLabel *queueLengthLabel = new QLabel(tr("Queue Length:"));
    queueLengthLayout->addWidget(queueLengthLabel);
    m_queueLengthEdit->setText("1");
    m_queueLengthEdit->setValidator(new QIntValidator(0, 999999, m_queueLengthEdit));
    queueLengthLayout->addWidget(m_queueLengthEdit);
    queueLengthLayout->addStretch();
    receiverComSpecLayout->addLayout(queueLengthLayout);
    
    // Transformation group box
    m_transformationGroup->setTitle(tr("Transformation"));
    m_transformationGroup->setStyleSheet("QGroupBox { border: 1px solid #ccc; border-radius: 3px; margin-top: 10px; padding-top: 10px; } QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; }");
    QFormLayout *transformationLayout = new QFormLayout(m_transformationGroup);
    m_transformationTypeCombo->addItem(tr("None"));
    m_transformationTypeCombo->setCurrentIndex(0);
    transformationLayout->addRow(tr("Type:"), m_transformationTypeCombo);
    transformationLayout->addRow(tr("Disabled:"), m_transformationDisabledCheck);
    
    // Transformation parameter fields
    transformationLayout->addRow(tr("Max Delta Counter:"), m_maxDeltaCounterEdit);
    transformationLayout->addRow(tr("Max Error State Init:"), m_maxErrorStateInitEdit);
    transformationLayout->addRow(tr("Max Error State Invalid:"), m_maxErrorStateInvalidEdit);
    transformationLayout->addRow(tr("Max Error State Valid:"), m_maxErrorStateValidEdit);
    transformationLayout->addRow(tr("Min Ok State Init:"), m_minOkStateInitEdit);
    transformationLayout->addRow(tr("Min Ok State Invalid:"), m_minOkStateInvalidEdit);
    transformationLayout->addRow(tr("Min Ok State Valid:"), m_minOkStateValidEdit);
    transformationLayout->addRow(tr("Sync Counter Init:"), m_syncCounterInitEdit);
    
    receiverComSpecLayout->addWidget(m_transformationGroup);
    receiverComSpecLayout->addStretch();
    
    // Client-Server ComSpec tab - for R-PORT-PROTOTYPE with CLIENT-SERVER-INTERFACE
    // Layout: Just the Transformation group box directly (no internal tabs)
    QVBoxLayout *clientServerMainLayout = new QVBoxLayout(m_clientServerComSpecTab);
    clientServerMainLayout->setContentsMargins(10, 10, 10, 10);
    clientServerMainLayout->setSpacing(15);
    
    // Transformation group box
    m_clientServerTransformationGroup->setTitle(tr("Transformation"));
    m_clientServerTransformationGroup->setStyleSheet("QGroupBox { border: 1px solid #ccc; border-radius: 3px; margin-top: 10px; padding-top: 10px; } QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; }");
    QGridLayout *clientServerTransformationLayout = new QGridLayout(m_clientServerTransformationGroup);
    
    // Type dropdown (left column, row 0)
    QLabel *typeLabel = new QLabel(tr("Type:"));
    clientServerTransformationLayout->addWidget(typeLabel, 0, 0);
    m_clientServerTransformationTypeCombo->addItem(tr("None"));
    m_clientServerTransformationTypeCombo->addItem(tr("End to End"));
    m_clientServerTransformationTypeCombo->addItem(tr("User"));
    m_clientServerTransformationTypeCombo->setCurrentIndex(0);
    clientServerTransformationLayout->addWidget(m_clientServerTransformationTypeCombo, 0, 1);
    
    // Disabled checkbox (right column, row 0)
    QLabel *disabledLabel = new QLabel(tr("Disabled:"));
    clientServerTransformationLayout->addWidget(disabledLabel, 0, 2);
    clientServerTransformationLayout->addWidget(m_clientServerTransformationDisabledCheck, 0, 3);
    
    // Left column fields
    QLabel *maxDeltaCounterLabel = new QLabel(tr("Max Delta Counter:"));
    clientServerTransformationLayout->addWidget(maxDeltaCounterLabel, 1, 0);
    clientServerTransformationLayout->addWidget(m_clientServerMaxDeltaCounterEdit, 1, 1);
    
    QLabel *maxErrorStateInitLabel = new QLabel(tr("Max Error State Init:"));
    clientServerTransformationLayout->addWidget(maxErrorStateInitLabel, 2, 0);
    clientServerTransformationLayout->addWidget(m_clientServerMaxErrorStateInitEdit, 2, 1);
    
    QLabel *maxErrorStateInvalidLabel = new QLabel(tr("Max Error State Invalid:"));
    clientServerTransformationLayout->addWidget(maxErrorStateInvalidLabel, 3, 0);
    clientServerTransformationLayout->addWidget(m_clientServerMaxErrorStateInvalidEdit, 3, 1);
    
    QLabel *maxErrorStateValidLabel = new QLabel(tr("Max Error State Valid:"));
    clientServerTransformationLayout->addWidget(maxErrorStateValidLabel, 4, 0);
    clientServerTransformationLayout->addWidget(m_clientServerMaxErrorStateValidEdit, 4, 1);
    
    QLabel *maxNoNewOrRepeatedDataLabel = new QLabel(tr("Max No New Or Repeated Data:"));
    clientServerTransformationLayout->addWidget(maxNoNewOrRepeatedDataLabel, 5, 0);
    clientServerTransformationLayout->addWidget(m_clientServerMaxNoNewOrRepeatedDataEdit, 5, 1);
    
    // Right column fields
    QLabel *minOkStateInitLabel = new QLabel(tr("Min Ok State Init:"));
    clientServerTransformationLayout->addWidget(minOkStateInitLabel, 1, 2);
    clientServerTransformationLayout->addWidget(m_clientServerMinOkStateInitEdit, 1, 3);
    
    QLabel *minOkStateInvalidLabel = new QLabel(tr("Min Ok State Invalid:"));
    clientServerTransformationLayout->addWidget(minOkStateInvalidLabel, 2, 2);
    clientServerTransformationLayout->addWidget(m_clientServerMinOkStateInvalidEdit, 2, 3);
    
    QLabel *minOkStateValidLabel = new QLabel(tr("Min Ok State Valid:"));
    clientServerTransformationLayout->addWidget(minOkStateValidLabel, 3, 2);
    clientServerTransformationLayout->addWidget(m_clientServerMinOkStateValidEdit, 3, 3);
    
    QLabel *syncCounterInitLabel = new QLabel(tr("Sync Counter Init:"));
    clientServerTransformationLayout->addWidget(syncCounterInitLabel, 4, 2);
    clientServerTransformationLayout->addWidget(m_clientServerSyncCounterInitEdit, 4, 3);
    
    QLabel *windowSizeLabel = new QLabel(tr("Window Size:"));
    clientServerTransformationLayout->addWidget(windowSizeLabel, 5, 2);
    clientServerTransformationLayout->addWidget(m_clientServerWindowSizeEdit, 5, 3);
    
    clientServerMainLayout->addWidget(m_clientServerTransformationGroup);
    clientServerMainLayout->addStretch();
    
    // Server ComSpec tab - for P-PORT-PROTOTYPE with PROVIDED-INTERFACE-TREF DEST="CLIENT-SERVER-INTERFACE"
    // Layout: Queue Length field above Transformation group box (similar to client-server but with Queue Length)
    QVBoxLayout *serverMainLayout = new QVBoxLayout(m_serverComSpecTab);
    serverMainLayout->setContentsMargins(10, 10, 10, 10);
    serverMainLayout->setSpacing(15);
    
    // Queue Length field (above Transformation)
    QHBoxLayout *serverQueueLengthLayout = new QHBoxLayout;
    QLabel *serverQueueLengthLabel = new QLabel(tr("Queue Length:"));
    m_serverQueueLengthEdit->setValidator(new QIntValidator(0, 999999, this));
    serverQueueLengthLayout->addWidget(serverQueueLengthLabel);
    serverQueueLengthLayout->addWidget(m_serverQueueLengthEdit);
    serverQueueLengthLayout->addStretch();
    serverMainLayout->addLayout(serverQueueLengthLayout);
    
    // Transformation group box (same as client-server)
    QGroupBox *serverTransformationGroup = new QGroupBox;
    serverTransformationGroup->setTitle(tr("Transformation"));
    serverTransformationGroup->setStyleSheet("QGroupBox { border: 1px solid #ccc; border-radius: 3px; margin-top: 10px; padding-top: 10px; } QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; }");
    QGridLayout *serverTransformationLayout = new QGridLayout(serverTransformationGroup);
    
    // Reuse client-server transformation widgets for server ports
    // Type dropdown (left column, row 0)
    QLabel *serverTypeLabel = new QLabel(tr("Type:"));
    serverTransformationLayout->addWidget(serverTypeLabel, 0, 0);
    QComboBox *serverTransformationTypeCombo = new QComboBox;
    serverTransformationTypeCombo->addItem(tr("None"));
    serverTransformationTypeCombo->addItem(tr("End to End"));
    serverTransformationTypeCombo->addItem(tr("User"));
    serverTransformationTypeCombo->setCurrentIndex(0);
    serverTransformationLayout->addWidget(serverTransformationTypeCombo, 0, 1);
    
    // Disabled checkbox (right column, row 0)
    QLabel *serverDisabledLabel = new QLabel(tr("Disabled:"));
    serverTransformationLayout->addWidget(serverDisabledLabel, 0, 2);
    QCheckBox *serverTransformationDisabledCheck = new QCheckBox;
    serverTransformationLayout->addWidget(serverTransformationDisabledCheck, 0, 3);
    
    // Left column fields
    QLabel *serverMaxDeltaCounterLabel = new QLabel(tr("Max Delta Counter:"));
    serverTransformationLayout->addWidget(serverMaxDeltaCounterLabel, 1, 0);
    QLineEdit *serverMaxDeltaCounterEdit = new QLineEdit;
    serverTransformationLayout->addWidget(serverMaxDeltaCounterEdit, 1, 1);
    
    QLabel *serverMaxErrorStateInitLabel = new QLabel(tr("Max Error State Init:"));
    serverTransformationLayout->addWidget(serverMaxErrorStateInitLabel, 2, 0);
    QLineEdit *serverMaxErrorStateInitEdit = new QLineEdit;
    serverTransformationLayout->addWidget(serverMaxErrorStateInitEdit, 2, 1);
    
    QLabel *serverMaxErrorStateInvalidLabel = new QLabel(tr("Max Error State Invalid:"));
    serverTransformationLayout->addWidget(serverMaxErrorStateInvalidLabel, 3, 0);
    QLineEdit *serverMaxErrorStateInvalidEdit = new QLineEdit;
    serverTransformationLayout->addWidget(serverMaxErrorStateInvalidEdit, 3, 1);
    
    QLabel *serverMaxErrorStateValidLabel = new QLabel(tr("Max Error State Valid:"));
    serverTransformationLayout->addWidget(serverMaxErrorStateValidLabel, 4, 0);
    QLineEdit *serverMaxErrorStateValidEdit = new QLineEdit;
    serverTransformationLayout->addWidget(serverMaxErrorStateValidEdit, 4, 1);
    
    QLabel *serverMaxNoNewOrRepeatedDataLabel = new QLabel(tr("Max No New Or Repeated Data:"));
    serverTransformationLayout->addWidget(serverMaxNoNewOrRepeatedDataLabel, 5, 0);
    QLineEdit *serverMaxNoNewOrRepeatedDataEdit = new QLineEdit;
    serverTransformationLayout->addWidget(serverMaxNoNewOrRepeatedDataEdit, 5, 1);
    
    // Right column fields
    QLabel *serverMinOkStateInitLabel = new QLabel(tr("Min Ok State Init:"));
    serverTransformationLayout->addWidget(serverMinOkStateInitLabel, 1, 2);
    QLineEdit *serverMinOkStateInitEdit = new QLineEdit;
    serverTransformationLayout->addWidget(serverMinOkStateInitEdit, 1, 3);
    
    QLabel *serverMinOkStateInvalidLabel = new QLabel(tr("Min Ok State Invalid:"));
    serverTransformationLayout->addWidget(serverMinOkStateInvalidLabel, 2, 2);
    QLineEdit *serverMinOkStateInvalidEdit = new QLineEdit;
    serverTransformationLayout->addWidget(serverMinOkStateInvalidEdit, 2, 3);
    
    QLabel *serverMinOkStateValidLabel = new QLabel(tr("Min Ok State Valid:"));
    serverTransformationLayout->addWidget(serverMinOkStateValidLabel, 3, 2);
    QLineEdit *serverMinOkStateValidEdit = new QLineEdit;
    serverTransformationLayout->addWidget(serverMinOkStateValidEdit, 3, 3);
    
    QLabel *serverSyncCounterInitLabel = new QLabel(tr("Sync Counter Init:"));
    serverTransformationLayout->addWidget(serverSyncCounterInitLabel, 4, 2);
    QLineEdit *serverSyncCounterInitEdit = new QLineEdit;
    serverTransformationLayout->addWidget(serverSyncCounterInitEdit, 4, 3);
    
    QLabel *serverWindowSizeLabel = new QLabel(tr("Window Size:"));
    serverTransformationLayout->addWidget(serverWindowSizeLabel, 5, 2);
    QLineEdit *serverWindowSizeEdit = new QLineEdit;
    serverTransformationLayout->addWidget(serverWindowSizeEdit, 5, 3);
    
    serverMainLayout->addWidget(serverTransformationGroup);
    serverMainLayout->addStretch();
    
    // Interface Properties tab
    QVBoxLayout *interfacePropertiesLayout = new QVBoxLayout(m_interfacePropertiesTab);
    interfacePropertiesLayout->setContentsMargins(10, 10, 10, 10);
    interfacePropertiesLayout->setSpacing(15);
    
    // Name field
    QHBoxLayout *interfaceNameLayout = new QHBoxLayout;
    QLabel *interfaceNameLabel = new QLabel(tr("Name:"));
    interfaceNameLayout->addWidget(interfaceNameLabel);
    m_interfacePropNameEdit->setText(tr("De"));
    interfaceNameLayout->addWidget(m_interfacePropNameEdit);
    interfacePropertiesLayout->addLayout(interfaceNameLayout);
    
    // Data Type group box
    m_dataTypeGroup->setTitle(tr("Data Type"));
    m_dataTypeGroup->setStyleSheet("QGroupBox { border: 1px solid #ccc; border-radius: 3px; margin-top: 10px; padding-top: 10px; } QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; }");
    QHBoxLayout *dataTypeLayout = new QHBoxLayout(m_dataTypeGroup);
    m_dataTypeEdit->setText(tr("Idt"));
    // Make it look like the Name field above (normal editable appearance, not dimmed)
    dataTypeLayout->addWidget(m_dataTypeEdit);
    // Three small square buttons horizontally aligned
    m_dataTypeButton1->setText(tr("..."));
    m_dataTypeButton1->setFixedSize(24, 24);
    m_dataTypeButton2->setIcon(QIcon::fromTheme("lightbulb"));
    m_dataTypeButton2->setFixedSize(24, 24);
    m_dataTypeButton3->setIcon(QIcon::fromTheme("document-open"));
    m_dataTypeButton3->setFixedSize(24, 24);
    dataTypeLayout->addWidget(m_dataTypeButton1);
    dataTypeLayout->addWidget(m_dataTypeButton2);
    dataTypeLayout->addWidget(m_dataTypeButton3);
    dataTypeLayout->addStretch();
    interfacePropertiesLayout->addWidget(m_dataTypeGroup);
    
    // Data Constraint
    QHBoxLayout *dataConstraintLayout = new QHBoxLayout;
    QLabel *dataConstraintLabel = new QLabel(tr("Data Constraint:"));
    dataConstraintLayout->addWidget(dataConstraintLabel);
    m_dataConstraintEdit->setText(tr("<None>"));
    dataConstraintLayout->addWidget(m_dataConstraintEdit);
    m_dataConstraintButton->setText(tr("..."));
    m_dataConstraintButton->setFixedSize(24, 24);
    dataConstraintLayout->addWidget(m_dataConstraintButton);
    dataConstraintLayout->addStretch();
    interfacePropertiesLayout->addLayout(dataConstraintLayout);
    
    // Use queued communication checkbox
    m_useQueuedCommCheck->setText(tr("Use queued communication"));
    m_useQueuedCommCheck->setChecked(false);
    interfacePropertiesLayout->addWidget(m_useQueuedCommCheck);
    
    // Measurement&Calibration group box
    m_measurementCalibrationGroup->setTitle(tr("Measurement&Calibration"));
    m_measurementCalibrationGroup->setStyleSheet("QGroupBox { border: 1px solid #ccc; border-radius: 3px; margin-top: 10px; padding-top: 10px; } QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; }");
    QFormLayout *measurementLayout = new QFormLayout(m_measurementCalibrationGroup);
    m_calibrationAccessCombo->addItem(tr("NotAccessible"));
    m_calibrationAccessCombo->addItem(tr("ReadOnly"));
    m_calibrationAccessCombo->addItem(tr("ReadWrite"));
    m_calibrationAccessCombo->setCurrentIndex(0);  // Default to "NotAccessible"
    measurementLayout->addRow(tr("Calibration Access:"), m_calibrationAccessCombo);
    interfacePropertiesLayout->addWidget(m_measurementCalibrationGroup);
    
    // Handle Invalid group box
    m_handleInvalidGroup->setTitle(tr("Handle Invalid"));
    m_handleInvalidGroup->setStyleSheet("QGroupBox { border: 1px solid #ccc; border-radius: 3px; margin-top: 10px; padding-top: 10px; } QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; }");
    QVBoxLayout *handleInvalidLayout = new QVBoxLayout(m_handleInvalidGroup);
    m_handleInvalidKeepRadio->setText(tr("Keep"));
    m_handleInvalidReplaceRadio->setText(tr("Replace"));
    m_handleInvalidNoneRadio->setText(tr("None"));
    m_handleInvalidNoneRadio->setChecked(true);  // "None" is checked by default
    m_handleInvalidButtonGroup->addButton(m_handleInvalidKeepRadio, 0);
    m_handleInvalidButtonGroup->addButton(m_handleInvalidReplaceRadio, 1);
    m_handleInvalidButtonGroup->addButton(m_handleInvalidNoneRadio, 2);
    handleInvalidLayout->addWidget(m_handleInvalidKeepRadio);
    handleInvalidLayout->addWidget(m_handleInvalidReplaceRadio);
    handleInvalidLayout->addWidget(m_handleInvalidNoneRadio);
    handleInvalidLayout->addStretch();
    interfacePropertiesLayout->addWidget(m_handleInvalidGroup);
    
    interfacePropertiesLayout->addStretch();
    
    // Description tab (placeholder for now)
    QVBoxLayout *commSpecDescLayout = new QVBoxLayout(m_commSpecDescriptionTab);
    commSpecDescLayout->setContentsMargins(10, 10, 10, 10);
    QLabel *commSpecDescLabel = new QLabel(tr("Description - functionality to be added"));
    commSpecDescLabel->setAlignment(Qt::AlignCenter);
    commSpecDescLayout->addWidget(commSpecDescLabel);
    
    // Tabs will be added dynamically based on port type in populatePortsTabs
    
    // Add sub-tabs to the container
    rightPanelLayout->addWidget(m_commSpecSubTabs);
    m_commSpecSubTabs->setVisible(false);  // Hide sub-tabs initially
    
    commSpecSplitter->addWidget(rightPanelContainer);
    
    // Set splitter sizes (left panel: 1/3, right panel: 2/3)
    commSpecSplitter->setStretchFactor(0, 1);
    commSpecSplitter->setStretchFactor(1, 2);
    commSpecSplitter->setSizes(QList<int>() << 200 << 400);
    
    commSpecMainLayout->addWidget(commSpecSplitter);
    
    // Setup Description tab - text input panel
    m_portsDescriptionTab->setPlaceholderText(tr("Enter description text here..."));
}

void MainWindow::populatePortsTabs(ArxmlElement* elem)
{
    // Block signals during population
    m_portNameEdit->blockSignals(true);
    m_directionRadio1->blockSignals(true);
    m_directionRadio2->blockSignals(true);
    m_directionRadio3->blockSignals(true);
    m_enableIndirectApiCheck->blockSignals(true);
    m_enableApiUsageByAddressCheck->blockSignals(true);
    m_transformationErrorHandlingCheck->blockSignals(true);
    m_portDefinedArgsTable->blockSignals(true);
    m_commSpecDeElementsList->blockSignals(true);
    m_initValueEdit->blockSignals(true);
    m_initValueTypeCombo->blockSignals(true);
    m_usesTxAcknowledgeCheck->blockSignals(true);
    m_timeoutEdit->blockSignals(true);
    m_timeoutUnitCombo->blockSignals(true);
    m_usesEndToEndProtectionCheck->blockSignals(true);
    m_portsDescriptionTab->blockSignals(true);
    
    // Populate Properties tab - find SHORT-NAME
    QString portName;
    for (const auto& child : elem->children) {
        if (child->tagName.compare("SHORT-NAME", Qt::CaseInsensitive) == 0) {
            portName = child->text;
            break;
        }
    }
    m_portNameEdit->setText(portName);
    
    // Find REQUIRED-INTERFACE-TREF or PROVIDED-INTERFACE-TREF for Port Interface name
    QString interfaceName;
    QString interfaceFullPath;  // Store the full path for tooltip
    QString interfaceDest;
    bool isProvidedInterface = false;
    bool isRequiredInterface = false;
    std::shared_ptr<ArxmlElement> interfaceTrefElement = nullptr;
    
    for (const auto& child : elem->children) {
        QString tagLower = child->tagName.toLower();
        if (tagLower.contains("provided-interface-tref")) {
            isProvidedInterface = true;
            interfaceTrefElement = child;
            // Get the DEST attribute
            for (const auto& attr : child->attributes) {
                if (attr.first.compare("DEST", Qt::CaseInsensitive) == 0) {
                    interfaceDest = attr.second;
                    break;
                }
            }
            // Try to get the text content as interface name (full path)
            interfaceFullPath = child->text;
            if (interfaceFullPath.isEmpty()) {
                // Try to find SHORT-NAME child
                for (const auto& grandchild : child->children) {
                    if (grandchild->tagName.compare("SHORT-NAME", Qt::CaseInsensitive) == 0) {
                        interfaceFullPath = grandchild->text;
                        break;
                    }
                }
            }
            break;
        } else if (tagLower.contains("required-interface-tref")) {
            isRequiredInterface = true;
            interfaceTrefElement = child;
            // Get the DEST attribute
            for (const auto& attr : child->attributes) {
                if (attr.first.compare("DEST", Qt::CaseInsensitive) == 0) {
                    interfaceDest = attr.second;
                    break;
                }
            }
            // Try to get the text content as interface name (full path)
            interfaceFullPath = child->text;
            if (interfaceFullPath.isEmpty()) {
                // Try to find SHORT-NAME child
                for (const auto& grandchild : child->children) {
                    if (grandchild->tagName.compare("SHORT-NAME", Qt::CaseInsensitive) == 0) {
                        interfaceFullPath = grandchild->text;
                        break;
                    }
                }
            }
            break;
        }
    }
    
    // Extract only the last name from the path (split by '/' and take the last part)
    if (!interfaceFullPath.isEmpty()) {
        QStringList pathParts = interfaceFullPath.split('/', Qt::SkipEmptyParts);
        if (!pathParts.isEmpty()) {
            interfaceName = pathParts.last();
        } else {
            interfaceName = interfaceFullPath;  // Fallback if no '/' found
        }
    }
    
    // Set the short name in the text box
    m_portInterfaceNameEdit->setText(interfaceName);
    
    // Set tooltip with Port Interface name and Package (full path)
    // Align colons by padding "Package:" to match "Port Interface:" width
    if (!interfaceFullPath.isEmpty()) {
        QString packageLabel = "Package:";
        QString portInterfaceLabel = "Port Interface:";
        // Calculate padding needed to align colons
        int paddingLength = portInterfaceLabel.length() - packageLabel.length();
        QString padding = QString(paddingLength, ' ');
        
        QString tooltipText = QString("%1 %2\n%3%4 %5")
            .arg(portInterfaceLabel)
            .arg(interfaceName)
            .arg(packageLabel)
            .arg(padding)
            .arg(interfaceFullPath);
        m_portInterfaceNameEdit->setToolTip(tooltipText);
    } else {
        m_portInterfaceNameEdit->setToolTip(QString());
    }
    
    // Determine port type and direction
    bool isPPortPrototype = elem->tagName.compare("P-PORT-PROTOTYPE", Qt::CaseInsensitive) == 0;
    bool isRPortPrototype = elem->tagName.compare("R-PORT-PROTOTYPE", Qt::CaseInsensitive) == 0;
    
    // Check if there's a stored DIRECTION element
    QString storedDirection;
    for (const auto& child : elem->children) {
        if (child->tagName.compare("DIRECTION", Qt::CaseInsensitive) == 0) {
            storedDirection = child->text;
            break;
        }
    }
    
    // Set direction radio buttons based on interface type
    m_directionRadio1->setChecked(false);
    m_directionRadio2->setChecked(false);
    m_directionRadio3->setChecked(false);
    
    if (interfaceDest.contains("CLIENT-SERVER-INTERFACE", Qt::CaseInsensitive)) {
        m_directionRadio1->setText(tr("Server"));
        m_directionRadio2->setText(tr("Client"));
        m_directionRadio3->setText(tr("Client/Server"));
        
        // Check stored direction first
        if (!storedDirection.isEmpty()) {
            if (storedDirection == tr("Server")) {
                m_directionRadio1->setChecked(true);
            } else if (storedDirection == tr("Client")) {
                m_directionRadio2->setChecked(true);
            } else if (storedDirection == tr("Client/Server")) {
                m_directionRadio3->setChecked(true);
            }
        } else {
            // For R-PORT-PROTOTYPE with REQUIRED-INTERFACE-TREF DEST="CLIENT-SERVER-INTERFACE",
            // the direction is "Client" (second radio button) by default
            if (isRPortPrototype && isRequiredInterface) {
                m_directionRadio2->setChecked(true);
            }
        }
    } else if (interfaceDest.contains("SENDER-RECEIVER-INTERFACE", Qt::CaseInsensitive)) {
        m_directionRadio1->setText(tr("Sender"));
        m_directionRadio2->setText(tr("Receiver"));
        m_directionRadio3->setText(tr("Sender/Receiver"));
        
        // Check stored direction first
        if (!storedDirection.isEmpty()) {
            if (storedDirection == tr("Sender")) {
                m_directionRadio1->setChecked(true);
            } else if (storedDirection == tr("Receiver")) {
                m_directionRadio2->setChecked(true);
            } else if (storedDirection == tr("Sender/Receiver")) {
                m_directionRadio3->setChecked(true);
            }
        } else {
            // For P-PORT-PROTOTYPE with PROVIDED-INTERFACE-TREF DEST="SENDER-RECEIVER-INTERFACE", 
            // the direction is "Sender" (first radio button) by default
            // This includes server ports (which are also P-PORT-PROTOTYPE with PROVIDED and SENDER-RECEIVER-INTERFACE)
            if (isPPortPrototype && isProvidedInterface) {
                m_directionRadio1->setChecked(true);
            }
            // For R-PORT-PROTOTYPE with REQUIRED-INTERFACE-TREF DEST="SENDER-RECEIVER-INTERFACE",
            // the direction is typically "Receiver" (second radio button) by default
            else if (isRPortPrototype && isRequiredInterface) {
                m_directionRadio2->setChecked(true);
            }
        }
    } else {
        // Default labels if interface type is unknown
        m_directionRadio1->setText(tr("Option 1"));
        m_directionRadio2->setText(tr("Option 2"));
        m_directionRadio3->setText(tr("Option 3"));
    }
    
    // Unblock signals
    m_portNameEdit->blockSignals(false);
    m_directionRadio1->blockSignals(false);
    m_directionRadio2->blockSignals(false);
    m_directionRadio3->blockSignals(false);
    m_enableIndirectApiCheck->blockSignals(false);
    m_enableApiUsageByAddressCheck->blockSignals(false);
    m_transformationErrorHandlingCheck->blockSignals(false);
    m_portDefinedArgsTable->blockSignals(false);
    m_commSpecDeElementsList->blockSignals(false);
    m_initValueEdit->blockSignals(false);
    m_initValueTypeCombo->blockSignals(false);
    m_usesTxAcknowledgeCheck->blockSignals(false);
    m_timeoutEdit->blockSignals(false);
    m_timeoutUnitCombo->blockSignals(false);
    m_usesEndToEndProtectionCheck->blockSignals(false);
    
    // Populate Data Elements list from PROVIDED-COM-SPECS or REQUIRED-COM-SPECS
    m_commSpecDeElementsList->clear();
    
    // Find PROVIDED-COM-SPECS or REQUIRED-COM-SPECS
    std::shared_ptr<ArxmlElement> comSpecsElement = nullptr;
    for (const auto& child : elem->children) {
        QString tagLower = child->tagName.toLower();
        if (tagLower.contains("provided-com-specs") || tagLower.contains("required-com-specs")) {
            comSpecsElement = child;
            break;
        }
    }
    
    // Clear the mapping
    m_dataElementToComSpec.clear();
    
    if (comSpecsElement) {
        // Find NONQUEUED-SENDER-COM-SPEC, QUEUED-SENDER-COM-SPEC, NONQUEUED-RECEIVER-COM-SPEC, etc.
        for (const auto& comSpec : comSpecsElement->children) {
            QString comSpecTagLower = comSpec->tagName.toLower();
            if (comSpecTagLower.contains("sender-com-spec") || comSpecTagLower.contains("receiver-com-spec")) {
                // Find DATA-ELEMENT-REF
                for (const auto& comSpecChild : comSpec->children) {
                    QString childTagLower = comSpecChild->tagName.toLower();
                    if (childTagLower.contains("data-element-ref")) {
                        QString dataElementPath = comSpecChild->text;
                        if (!dataElementPath.isEmpty()) {
                            // Extract last name from path
                            QStringList pathParts = dataElementPath.split('/', Qt::SkipEmptyParts);
                            if (!pathParts.isEmpty()) {
                                QString dataElementName = pathParts.last();
                                // Store the data element name and associate it with the COM-SPEC
                                m_commSpecDeElementsList->addItem(dataElementName);
                                m_dataElementToComSpec[dataElementName] = comSpec;
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Populate Description tab - set text content
    QString descriptionText;
    if (!elem->text.isEmpty()) {
        descriptionText = elem->text;
    } else {
        // Look for description-related children
        for (const auto& child : elem->children) {
            QString tagLower = child->tagName.toLower();
            if (tagLower.contains("desc") || tagLower.contains("annotation") ||
                tagLower.contains("documentation")) {
                if (!descriptionText.isEmpty()) {
                    descriptionText += "\n";
                }
                descriptionText += child->text.isEmpty() ? child->tagName : child->text;
            }
        }
    }
    m_portsDescriptionTab->setPlainText(descriptionText);
    
    // Setup Communication Spec sub-tabs based on port type
    m_commSpecSubTabs->clear();  // Clear existing tabs
    
    bool isRPortWithSenderReceiver = isRPortPrototypeWithSenderReceiver(elem);
    bool isRPortWithClientServer = isRPortPrototypeWithClientServer(elem);
    bool isPPortWithClientServer = isPPortPrototypeClientServer(elem);
    
    if (isRPortWithSenderReceiver) {
        // For R-PORT-PROTOTYPE with SENDER-RECEIVER-INTERFACE, show Receiver ComSpec
        m_commSpecSubTabs->addTab(m_receiverComSpecTab, tr("Receiver ComSpec"));
        // Add common tabs
        m_commSpecSubTabs->addTab(m_interfacePropertiesTab, tr("Interface Properties"));
        m_commSpecSubTabs->addTab(m_commSpecDescriptionTab, tr("Description"));
    } else if (isRPortWithClientServer) {
        // For R-PORT-PROTOTYPE with CLIENT-SERVER-INTERFACE, show Client-Server ComSpec
        // Add Communication Spec tab (with Transformation) and Description tab directly
        m_commSpecSubTabs->addTab(m_clientServerComSpecTab, tr("Communication Spec"));
        m_commSpecSubTabs->addTab(m_commSpecDescriptionTab, tr("Description"));
    } else if (isPPortWithClientServer) {
        // For P-PORT-PROTOTYPE with PROVIDED-INTERFACE-TREF DEST="CLIENT-SERVER-INTERFACE",
        // show Server ComSpec (with Queue Length and Transformation) and Description tab
        m_commSpecSubTabs->addTab(m_serverComSpecTab, tr("Communication Spec"));
        m_commSpecSubTabs->addTab(m_commSpecDescriptionTab, tr("Description"));
    } else {
        // For P-PORT-PROTOTYPE with PROVIDED-INTERFACE-TREF DEST="SENDER-RECEIVER-INTERFACE" or other types,
        // show Sender ComSpec (default for sender-receiver provider ports)
        m_commSpecSubTabs->addTab(m_senderComSpecTab, tr("Sender ComSpec"));
        // Add common tabs
        m_commSpecSubTabs->addTab(m_interfacePropertiesTab, tr("Interface Properties"));
        m_commSpecSubTabs->addTab(m_commSpecDescriptionTab, tr("Description"));
    }
    
    // Unblock signals
    m_portsCommSpecTab->blockSignals(false);
    m_portsDescriptionTab->blockSignals(false);
}

bool MainWindow::isPortsElement(ArxmlElement* elem) const
{
    if (!elem) {
        return false;
    }
    
    // Check if parent is PORTS
    ArxmlElement* parent = elem->parent;
    if (!parent) {
        return false;
    }
    
    return parent->tagName.compare("PORTS", Qt::CaseInsensitive) == 0;
}

bool MainWindow::isRPortPrototypeWithSenderReceiver(ArxmlElement* elem) const
{
    if (!elem) {
        return false;
    }
    
    // Check if tag name is R-PORT-PROTOTYPE
    if (elem->tagName.compare("R-PORT-PROTOTYPE", Qt::CaseInsensitive) != 0) {
        return false;
    }
    
    // Check if it has REQUIRED-INTERFACE-TREF with DEST="SENDER-RECEIVER-INTERFACE"
    for (const auto& child : elem->children) {
        QString tagLower = child->tagName.toLower();
        if (tagLower.contains("required-interface-tref")) {
            // Check DEST attribute
            for (const auto& attr : child->attributes) {
                if (attr.first.compare("DEST", Qt::CaseInsensitive) == 0) {
                    if (attr.second.contains("SENDER-RECEIVER-INTERFACE", Qt::CaseInsensitive)) {
                        return true;
                    }
                }
            }
        }
    }
    
    return false;
}

bool MainWindow::isRPortPrototypeWithClientServer(ArxmlElement* elem) const
{
    if (!elem) {
        return false;
    }
    
    // Check if tag name is R-PORT-PROTOTYPE
    if (elem->tagName.compare("R-PORT-PROTOTYPE", Qt::CaseInsensitive) != 0) {
        return false;
    }
    
    // Check if it has REQUIRED-INTERFACE-TREF with DEST="CLIENT-SERVER-INTERFACE"
    for (const auto& child : elem->children) {
        QString tagLower = child->tagName.toLower();
        if (tagLower.contains("required-interface-tref")) {
            // Check DEST attribute
            for (const auto& attr : child->attributes) {
                if (attr.first.compare("DEST", Qt::CaseInsensitive) == 0) {
                    if (attr.second.contains("CLIENT-SERVER-INTERFACE", Qt::CaseInsensitive)) {
                        return true;
                    }
                }
            }
        }
    }
    
    return false;
}

bool MainWindow::isPPortPrototypeClientServer(ArxmlElement* elem) const
{
    if (!elem) {
        return false;
    }
    
    // Check if tag name is P-PORT-PROTOTYPE
    if (elem->tagName.compare("P-PORT-PROTOTYPE", Qt::CaseInsensitive) != 0) {
        return false;
    }
    
    // Check if it has PROVIDED-INTERFACE-TREF with DEST="CLIENT-SERVER-INTERFACE"
    for (const auto& child : elem->children) {
        QString tagLower = child->tagName.toLower();
        if (tagLower.contains("provided-interface-tref")) {
            // Check DEST attribute
            for (const auto& attr : child->attributes) {
                if (attr.first.compare("DEST", Qt::CaseInsensitive) == 0) {
                    QString destValue = attr.second;
                    QString destLower = destValue.toLower();
                    if (destLower.contains("client-server-interface")) {
                        return true;
                    }
                }
            }
        }
    }
    
    return false;
}

void MainWindow::onPropertyItemChanged(QTableWidgetItem *item)
{
    if (!item)
        return;

    QTreeWidgetItem *current = m_treeWidget->currentItem();
    if (!current)
        return;

    auto elem = getElementForItem(current);
    if (!elem)
        return;

    bool isPorts = isPortsElement(elem.get());
    
    if (isPorts) {
        // Determine which tab the edit came from by checking which table widget sent the signal
        // We'll need to track this differently - check all PORTS tables
        QString tabName;
        QTableWidget* sourceTable = nullptr;
        
        // Check each table to see which one contains this item
        // Since we don't have direct access, we'll use the sender() mechanism
        QObject* senderObj = sender();
        sourceTable = qobject_cast<QTableWidget*>(senderObj);
        
        // Properties tab is now a form widget, not a table, so skip it here
        // Port API Options tab is also a form widget now, skip it here
        // Port Communication Spec tab is also a form widget now, skip it here
        // Description tab is now a QTextEdit, handled separately
    } else {
        // Standard 2-column layout (Property | Value)
        if (item->column() != 1)
            return;

        QTableWidgetItem *fieldItem = m_propertyTable->item(item->row(), 0);
        if (!fieldItem)
            return;

        QString fieldName = fieldItem->text();
        QString newValue = item->text();

        // Update attribute or text
        if (fieldName != tr("Name") && fieldName != tr("Text")) {
            // Update attribute
            bool found = false;
            for (auto& attr : elem->attributes) {
                if (attr.first == fieldName) {
                    attr.second = newValue;
                    found = true;
                    break;
                }
            }
            if (!found) {
                elem->attributes.push_back({fieldName, newValue});
            }
            logAction(tr("Modified attribute '%1' for element '%2'").arg(fieldName).arg(elem->tagName));
        } else if (fieldName == tr("Text")) {
            elem->text = newValue;
            logAction(tr("Modified text for element '%1'").arg(elem->tagName));
        }
        
        // Refresh tree item display
        refreshTreeItem(current, elem.get());
    }
}

void MainWindow::onPortPropertyChanged()
{
    // Only process if signals are not blocked (i.e., user edit, not programmatic)
    if (m_portNameEdit->signalsBlocked())
        return;
    
    QTreeWidgetItem *current = m_treeWidget->currentItem();
    if (!current)
        return;

    auto elem = getElementForItem(current);
    if (!elem)
        return;
    
    // Only update if this is a PORTS element
    if (!isPortsElement(elem.get()))
        return;
    
    QString newName = m_portNameEdit->text();
    
    // Find and update SHORT-NAME child
    for (auto& child : elem->children) {
        if (child->tagName.compare("SHORT-NAME", Qt::CaseInsensitive) == 0) {
            if (child->text != newName) {
                child->text = newName;
                logAction(tr("Modified port name to '%1'").arg(newName));
                refreshTreeItem(current, elem.get());
            }
            return;
        }
    }
    
    // If SHORT-NAME doesn't exist, create it
    auto shortName = std::make_shared<ArxmlElement>();
    shortName->tagName = "SHORT-NAME";
    shortName->text = newName;
    shortName->parent = elem.get();
    elem->children.push_back(shortName);
    logAction(tr("Created SHORT-NAME with value '%1'").arg(newName));
    refreshTreeItem(current, elem.get());
}

void MainWindow::onDirectionChanged(int id)
{
    QTreeWidgetItem *current = m_treeWidget->currentItem();
    if (!current)
        return;

    auto elem = getElementForItem(current);
    if (!elem)
        return;
    
    // Only process if this is a PORTS element
    if (!isPortsElement(elem.get()))
        return;
    
    // Determine which radio button was selected and get its text
    QString directionText;
    if (id == 0) {
        directionText = m_directionRadio1->text();
    } else if (id == 1) {
        directionText = m_directionRadio2->text();
    } else if (id == 2) {
        directionText = m_directionRadio3->text();
    }
    
    // Find or create a DIRECTION child element to store the direction
    std::shared_ptr<ArxmlElement> directionElement = nullptr;
    for (const auto& child : elem->children) {
        if (child->tagName.compare("DIRECTION", Qt::CaseInsensitive) == 0) {
            directionElement = child;
            break;
        }
    }
    
    if (!directionElement) {
        // Create new DIRECTION element
        directionElement = elem->createChild("DIRECTION");
    }
    
    // Update the direction text
    directionElement->text = directionText;
    
    logAction(tr("Changed direction for port '%1' to '%2'").arg(elem->tagName).arg(directionText));
    
    // Refresh tree item display
    refreshTreeItem(current, elem.get());
}

void MainWindow::onCommSpecDeElementSelected()
{
    // Show sub-tabs if an item is selected, otherwise show "No properties available"
    bool hasSelection = m_commSpecDeElementsList->currentItem() != nullptr;
    m_commSpecSubTabs->setVisible(hasSelection);
    m_commSpecNoPropertiesLabel->setVisible(!hasSelection);
    
    if (!hasSelection) {
        // Clear Init Value fields when nothing is selected
        m_initValueEdit->clear();
        m_initValueEdit->setReadOnly(false);
        m_initValueTypeCombo->setCurrentIndex(0); // Reset to default
        return;
    }
    
    // Get the selected data element name
    QListWidgetItem *selectedItem = m_commSpecDeElementsList->currentItem();
    if (!selectedItem) {
        return;
    }
    
    QString dataElementName = selectedItem->text();
    
    // Find the associated COM-SPEC element
    if (!m_dataElementToComSpec.contains(dataElementName)) {
        return;
    }
    
    std::shared_ptr<ArxmlElement> comSpec = m_dataElementToComSpec[dataElementName];
    if (!comSpec) {
        return;
    }
    
    // Block signals while populating
    m_initValueEdit->blockSignals(true);
    m_initValueTypeCombo->blockSignals(true);
    
    // Find INIT-VALUE child
    std::shared_ptr<ArxmlElement> initValueElement = nullptr;
    for (const auto& child : comSpec->children) {
        if (child->tagName.compare("INIT-VALUE", Qt::CaseInsensitive) == 0) {
            initValueElement = child;
            break;
        }
    }
    
    if (initValueElement) {
        // Find NUMERICAL-VALUE-SPECIFICATION, TEXTUAL-VALUE-SPECIFICATION, etc.
        for (const auto& valueSpec : initValueElement->children) {
            QString valueSpecTagLower = valueSpec->tagName.toLower();
            
            if (valueSpecTagLower.contains("numerical-value-specification")) {
                // Set dropdown to "Numeric"
                int numericIndex = m_initValueTypeCombo->findText(tr("Numeric"));
                if (numericIndex >= 0) {
                    m_initValueTypeCombo->setCurrentIndex(numericIndex);
                }
                
                // Find VALUE child
                QString valueText;
                for (const auto& valueChild : valueSpec->children) {
                    if (valueChild->tagName.compare("VALUE", Qt::CaseInsensitive) == 0) {
                        valueText = valueChild->text;
                        break;
                    }
                }
                
                // Set the value and make it read-only/dimmed
                m_initValueEdit->setText(valueText);
                m_initValueEdit->setReadOnly(true);
                m_initValueEdit->setStyleSheet("QLineEdit { background-color: #f0f0f0; color: #888888; }");
                
            } else if (valueSpecTagLower.contains("textual-value-specification")) {
                // Set dropdown to "Textual"
                int textualIndex = m_initValueTypeCombo->findText(tr("Textual"));
                if (textualIndex >= 0) {
                    m_initValueTypeCombo->setCurrentIndex(textualIndex);
                }
                
                // Find VALUE child
                QString valueText;
                for (const auto& valueChild : valueSpec->children) {
                    if (valueChild->tagName.compare("VALUE", Qt::CaseInsensitive) == 0) {
                        valueText = valueChild->text;
                        break;
                    }
                }
                
                // Set the value and make it read-only/dimmed
                m_initValueEdit->setText(valueText);
                m_initValueEdit->setReadOnly(true);
                m_initValueEdit->setStyleSheet("QLineEdit { background-color: #f0f0f0; color: #888888; }");
            }
            // Add more cases for Array, Record, Reference, Application, None as needed
        }
    } else {
        // No INIT-VALUE found, reset to default
        m_initValueEdit->clear();
        m_initValueEdit->setReadOnly(false);
        m_initValueEdit->setStyleSheet(""); // Reset style
        m_initValueTypeCombo->setCurrentIndex(0);
    }
    
    // Unblock signals
    m_initValueEdit->blockSignals(false);
    m_initValueTypeCombo->blockSignals(false);
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    // Handle mouse clicks on the Data Elements panel to clear selection
    if (event->type() == QEvent::MouseButtonPress) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
        
        // Find the Data Elements panel widget
        QWidget *deElementsPanel = nullptr;
        if (m_commSpecDeElementsList && m_commSpecDeElementsList->parentWidget()) {
            deElementsPanel = m_commSpecDeElementsList->parentWidget();
        }
        
        // Handle clicks on the panel (outside the list widget)
        if (obj == deElementsPanel && mouseEvent->button() == Qt::LeftButton) {
            // Check if click is outside the list widget
            QPoint globalPos = mouseEvent->globalPosition().toPoint();
            QPoint posInList = m_commSpecDeElementsList->mapFromGlobal(globalPos);
            if (!m_commSpecDeElementsList->rect().contains(posInList)) {
                // Click is outside the list widget, clear selection
                m_commSpecDeElementsList->setCurrentItem(nullptr);
                return true;  // Event handled
            }
        }
        
        // Handle clicks on the viewport (the area behind items) - this is the key!
        if (obj == m_commSpecDeElementsList->viewport() && mouseEvent->button() == Qt::LeftButton) {
            QPoint localPos = mouseEvent->pos();
            QListWidgetItem *item = m_commSpecDeElementsList->itemAt(localPos);
            if (!item) {
                // Clicked on empty space in the viewport, clear selection
                // Use setCurrentItem(nullptr) which reliably emits currentItemChanged signal
                m_commSpecDeElementsList->setCurrentItem(nullptr);
                return true;  // Event handled - don't let Qt process it
            }
        }
        
        // Also handle clicks directly on the list widget (fallback)
        if (obj == m_commSpecDeElementsList && mouseEvent->button() == Qt::LeftButton) {
            QPoint localPos = mouseEvent->pos();
            QListWidgetItem *item = m_commSpecDeElementsList->itemAt(localPos);
            if (!item) {
                // Clicked on empty space in the list
                m_commSpecDeElementsList->setCurrentItem(nullptr);
                return true;  // Event handled
            }
        }
    }
    
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::logAction(const QString &message)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    m_actionLog->append(QString("[%1] %2").arg(timestamp).arg(message));
}

void MainWindow::showContextMenu(const QPoint &pos)
{
    QTreeWidgetItem *item = m_treeWidget->itemAt(pos);
    if (!item)
        return;

    QMenu menu(this);
    menu.addAction(tr("Add Child Element"), this, &MainWindow::addChildElement);
    menu.addAction(tr("Delete Element"), this, &MainWindow::deleteElement);
    menu.exec(m_treeWidget->mapToGlobal(pos));
}

void MainWindow::addChildElement()
{
    QTreeWidgetItem *current = m_treeWidget->currentItem();
    if (!current)
        return;

    bool ok;
    QString tagName = QInputDialog::getText(this, tr("Add Child Element"),
                                           tr("Tag name:"), QLineEdit::Normal,
                                           "", &ok);
    if (!ok || tagName.isEmpty())
        return;

    auto elem = getElementForItem(current);
    if (!elem)
        return;

    auto newChild = std::make_shared<ArxmlElement>();
    newChild->tagName = tagName;
    newChild->parent = elem.get();
    elem->children.push_back(newChild);

    // Add to tree
    QTreeWidgetItem *childItem = new QTreeWidgetItem(current);
    childItem->setText(0, tagName);
    QList<int> indexPath = m_model->getElementIndexPath(newChild.get());
    childItem->setData(0, Qt::UserRole, QVariant::fromValue(indexPath));
    
    logAction(tr("Added child element '%1' to '%2'").arg(tagName).arg(elem->tagName));
}

void MainWindow::deleteElement()
{
    QTreeWidgetItem *current = m_treeWidget->currentItem();
    if (!current)
        return;

    auto elem = getElementForItem(current);
    if (!elem)
        return;

    QTreeWidgetItem *parentItem = current->parent();
    if (!parentItem) {
        QMessageBox::warning(this, tr("Cannot Delete"),
                            tr("Cannot delete root element."));
        return;
    }

    auto parentElem = getElementForItem(parentItem);
    if (!parentElem)
        return;

    // Remove from model
    parentElem->children.erase(
        std::remove_if(parentElem->children.begin(), parentElem->children.end(),
                      [&elem](const std::shared_ptr<ArxmlElement>& child) {
                          return child.get() == elem.get();
                      }),
        parentElem->children.end());

    // Remove from tree
    delete current;
    
    logAction(tr("Deleted element '%1'").arg(elem->tagName));
}

void MainWindow::validateDocument()
{
    if (!m_model->rootElement()) {
        QMessageBox::information(this, tr("No Document"),
                                tr("Please open an ARXML file before validating."));
        return;
    }

    // For now, validation without schema file - just check XML well-formedness
    // TODO: Add schema file selection dialog
    QString schemaFile = "";  // Empty schema file means we'll skip schema validation
    QString result;
    if (schemaFile.isEmpty()) {
        // Basic XML validation - just check if file can be saved and loaded
        QTemporaryFile tmpFile;
        if (!tmpFile.open()) {
            result = tr("Failed to create temporary file for validation.");
        } else {
            tmpFile.close();
            if (!m_model->saveToFile(tmpFile.fileName())) {
                result = tr("Failed to write document to temporary file.");
            } else {
                // Try to reload to verify well-formedness
                ArxmlModel testModel;
                if (!testModel.loadFromFile(tmpFile.fileName())) {
                    result = tr("Document is not well-formed XML.");
                }
            }
        }
    } else {
        result = m_validator->validate(*m_model, schemaFile);
    }
    
    if (result.isEmpty()) {
        logAction(tr("Document validation: PASSED"));
        QMessageBox::information(this, tr("Validation Passed"),
                                tr("The document is well-formed."));
    } else {
        logAction(tr("Document validation: FAILED"));
        logAction(tr("Validation errors:\n%1").arg(result));
        
        // Show detailed error in message box
        QMessageBox msgBox(this);
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setWindowTitle(tr("Validation Failed"));
        msgBox.setText(tr("The document validation failed."));
        msgBox.setDetailedText(result);
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.exec();
    }
}

std::shared_ptr<ArxmlElement> MainWindow::getElementForItem(QTreeWidgetItem *item)
{
    if (!item)
        return nullptr;

    // Get index path from variant
    QVariant v = item->data(0, Qt::UserRole);
    if (!v.canConvert<QList<int>>())
        return nullptr;

    // Get index path from variant
    QList<int> indexPath = v.value<QList<int>>();
    
    // Find element using index path
    return m_model->findElementByIndexPath(indexPath);
}

void MainWindow::onDescriptionTextChanged()
{
    QTreeWidgetItem *current = m_treeWidget->currentItem();
    if (!current)
        return;

    auto elem = getElementForItem(current);
    if (!elem)
        return;

    // Only update if this is a PORTS element
    if (!isPortsElement(elem.get()))
        return;

    // Update the element's text with the description
    QString newText = m_portsDescriptionTab->toPlainText();
    if (elem->text != newText) {
        elem->text = newText;
        logAction(tr("Modified description for element '%1'").arg(elem->tagName));
    }
}

void MainWindow::onSearchTextChanged(const QString &text)
{
    // Filter tree items based on search text
    filterTreeItems(text);
}

void MainWindow::filterTreeItems(const QString &searchText)
{
    if (searchText.isEmpty()) {
        // Show all items
        QTreeWidgetItemIterator it(m_treeWidget);
        while (*it) {
            (*it)->setHidden(false);
            ++it;
        }
        return;
    }

    QString searchLower = searchText.toLower();
    
    // Hide all items first
    QTreeWidgetItemIterator it(m_treeWidget);
    while (*it) {
        (*it)->setHidden(true);
        ++it;
    }

    // Show items that match and their parents/children
    QTreeWidgetItemIterator it2(m_treeWidget);
    while (*it2) {
        QTreeWidgetItem *item = *it2;
        
        // Check if item name or package matches
        QString name = item->text(0).toLower();
        QString package = item->text(1).toLower();
        
        if (name.contains(searchLower) || package.contains(searchLower)) {
            // Show this item
            item->setHidden(false);
            
            // Show all ancestors (parents)
            QTreeWidgetItem *parent = item->parent();
            while (parent) {
                parent->setHidden(false);
                parent->setExpanded(true);  // Expand to show matches
                parent = parent->parent();
            }
            
            // Show all descendants (children)
            showItemAndChildren(item);
        }
        
        ++it2;
    }
}

void MainWindow::showItemAndChildren(QTreeWidgetItem *item)
{
    if (!item)
        return;
    
    item->setHidden(false);
    item->setExpanded(true);  // Expand to show matches
    
    for (int i = 0; i < item->childCount(); ++i) {
        showItemAndChildren(item->child(i));
    }
}
