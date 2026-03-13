// main_window.hpp
//
// Main window with multi-panel layout similar to AUTOSAR tools

#ifndef MAIN_WINDOW_HPP
#define MAIN_WINDOW_HPP

#include <QMainWindow>
#include <memory>

class QTreeWidget;
class QTableWidget;
class QTableWidgetItem;
class QPushButton;
class QTreeWidgetItem;
class QTextEdit;
class QTabWidget;
class QWidget;
class QGroupBox;
class QLineEdit;
class QLabel;
class QVBoxLayout;
class QHBoxLayout;
class QRadioButton;
class QButtonGroup;
class QCheckBox;
class QComboBox;
class QListWidget;
class ArxmlModel;
class ArxmlValidator;
class ArxmlElement;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    // File operations
    void openFile();
    void saveFile();
    void saveFileAs();

    // Tree selection
    void onCurrentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);
    // Search filter
    void onSearchTextChanged(const QString &text);
    void filterTreeItems(const QString &searchText);
    void showItemAndChildren(QTreeWidgetItem *item);

    // Property table edits
    void onPropertyItemChanged(QTableWidgetItem *item);
    // Description text edit
    void onDescriptionTextChanged();
    
    // Port property form edits
    void onPortPropertyChanged();
    void onDirectionChanged(int id);
    
    // Communication Spec De element selection
    void onCommSpecDeElementSelected();

    // Context menu actions
    void showContextMenu(const QPoint &pos);
    void addChildElement();
    void deleteElement();

    // Validation
    void validateDocument();

private:
    // Build the tree view from the model
    void populateTree();
    
    // Populate property table/tabs based on selected element
    void populatePropertyTable(ArxmlElement* elem);
    
    // Populate standard property table
    void populateStandardPropertyTable(ArxmlElement* elem);
    
    // Populate PORTS-specific tabs
    void populatePortsTabs(ArxmlElement* elem);
    
    // Setup PORTS tabs configuration
    void setupPortsTabs();
    
    // Build tree recursively
    void buildTreeRecursive(const std::shared_ptr<ArxmlElement>& elem, QTreeWidgetItem* parentItem);
    
    // Refresh tree item display from element data
    void refreshTreeItem(QTreeWidgetItem* item, ArxmlElement* elem);
    
    // Get display name and package from element
    void getElementDisplayInfo(ArxmlElement* elem, QString& name, QString& package) const;
    
    // Check if element is a PORTS element or child of PORTS
    bool isPortsElement(ArxmlElement* elem) const;
    // Check if element is R-PORT-PROTOTYPE with SENDER-RECEIVER-INTERFACE
    bool isRPortPrototypeWithSenderReceiver(ArxmlElement* elem) const;
    // Check if element is R-PORT-PROTOTYPE with CLIENT-SERVER-INTERFACE
    bool isRPortPrototypeWithClientServer(ArxmlElement* elem) const;
    // Check if element is P-PORT-PROTOTYPE with PROVIDED-INTERFACE-TREF DEST="CLIENT-SERVER-INTERFACE" (server port)
    bool isPPortPrototypeClientServer(ArxmlElement* elem) const;
    
    // Update action log
    void logAction(const QString& message);

    // UI members
    QTreeWidget *m_treeWidget;
    QTabWidget *m_propertyTabWidget;
    QTableWidget *m_propertyTable;  // Standard property table
    QWidget *m_portsPropertiesTab;  // PORTS Properties tab (form widget)
    QWidget *m_portsApiOptionsTab;  // PORTS Port API Options tab (form widget)
    QWidget *m_portsCommSpecTab;    // PORTS Communication Spec tab (form widget)
    QTextEdit *m_portsDescriptionTab; // PORTS Description tab (text input panel)
    QTabWidget *m_logTabWidget;  // Tab widget for Action Log and Messages
    QTextEdit *m_actionLog;
    QWidget *m_messagesTab;  // Messages tab (placeholder for now)
    
    // Properties tab widgets
    QLineEdit *m_portNameEdit;
    QGroupBox *m_portInterfaceGroup;
    QLineEdit *m_portInterfaceNameEdit;
    QGroupBox *m_blueprintGroup;
    QLineEdit *m_blueprintNameEdit;
    QGroupBox *m_directionGroup;
    QButtonGroup *m_directionButtonGroup;
    QRadioButton *m_directionRadio1;
    QRadioButton *m_directionRadio2;
    QRadioButton *m_directionRadio3;
    
    // Port API Options tab widgets
    QGroupBox *m_apiOptionsGroup;
    QCheckBox *m_enableIndirectApiCheck;
    QCheckBox *m_enableApiUsageByAddressCheck;
    QCheckBox *m_transformationErrorHandlingCheck;
    QGroupBox *m_portDefinedArgsGroup;
    QTableWidget *m_portDefinedArgsTable;
    QPushButton *m_editArgsButton;
    QPushButton *m_getFromButton;
    QPushButton *m_copyToButton;
    
    // Port Communication Spec tab widgets
    QListWidget *m_commSpecDeElementsList;  // Left panel with De_... elements
    QTabWidget *m_commSpecSubTabs;  // Right panel sub-tabs (Sender/Receiver ComSpec, Interface Properties, Description)
    QWidget *m_senderComSpecTab;  // Sender ComSpec sub-tab with form fields
    QWidget *m_receiverComSpecTab;  // Receiver ComSpec sub-tab with form fields (for R-PORT-PROTOTYPE with SENDER-RECEIVER-INTERFACE)
    QWidget *m_clientServerComSpecTab;  // Client-Server ComSpec sub-tab (for R-PORT-PROTOTYPE with CLIENT-SERVER-INTERFACE)
    QWidget *m_serverComSpecTab;  // Server ComSpec sub-tab (for P-PORT-PROTOTYPE with PROVIDED-INTERFACE-TREF DEST="SENDER-RECEIVER-INTERFACE")
    QWidget *m_interfacePropertiesTab;  // Interface Properties sub-tab
    QWidget *m_commSpecDescriptionTab;  // Description sub-tab
    QLabel *m_commSpecNoPropertiesLabel;  // Label shown when no De element is selected
    // Receiver ComSpec widgets
    QLineEdit *m_receiverInitValueEdit;
    QComboBox *m_receiverInitValueTypeCombo;
    QPushButton *m_receiverInitValueEditButton;
    QButtonGroup *m_rxFilterButtonGroup;
    QRadioButton *m_rxFilterAlwaysRadio;
    QRadioButton *m_rxFilterMaskedRadio;
    QCheckBox *m_receiverUsesEndToEndProtectionCheck;
    QCheckBox *m_handleNeverReceivedCheck;
    QCheckBox *m_enableUpdateCheck;
    QLineEdit *m_aliveTimeoutEdit;
    QLineEdit *m_queueLengthEdit;
    QGroupBox *m_transformationGroup;
    QComboBox *m_transformationTypeCombo;
    QCheckBox *m_transformationDisabledCheck;
    QLineEdit *m_maxDeltaCounterEdit;
    QLineEdit *m_maxErrorStateInitEdit;
    QLineEdit *m_maxErrorStateInvalidEdit;
    QLineEdit *m_maxErrorStateValidEdit;
    QLineEdit *m_minOkStateInitEdit;
    QLineEdit *m_minOkStateInvalidEdit;
    QLineEdit *m_minOkStateValidEdit;
    QLineEdit *m_syncCounterInitEdit;
    // Client-Server ComSpec widgets (for R-PORT-PROTOTYPE with CLIENT-SERVER-INTERFACE)
    QListWidget *m_clientServerDeElementsList;  // Left panel with data elements
    QGroupBox *m_clientServerTransformationGroup;
    QComboBox *m_clientServerTransformationTypeCombo;
    QCheckBox *m_clientServerTransformationDisabledCheck;
    QLineEdit *m_clientServerMaxDeltaCounterEdit;
    QLineEdit *m_clientServerMaxErrorStateInitEdit;
    QLineEdit *m_clientServerMaxErrorStateInvalidEdit;
    QLineEdit *m_clientServerMaxErrorStateValidEdit;
    QLineEdit *m_clientServerMaxNoNewOrRepeatedDataEdit;
    QLineEdit *m_clientServerMinOkStateInitEdit;
    QLineEdit *m_clientServerMinOkStateInvalidEdit;
    QLineEdit *m_clientServerMinOkStateValidEdit;
    QLineEdit *m_clientServerSyncCounterInitEdit;
    QLineEdit *m_clientServerWindowSizeEdit;
    QLabel *m_clientServerNoPropertiesLabel;  // Label shown when no De element is selected
    // Server ComSpec widgets (for P-PORT-PROTOTYPE with PROVIDED-INTERFACE-TREF DEST="SENDER-RECEIVER-INTERFACE")
    QLineEdit *m_serverQueueLengthEdit;  // Queue Length field for server ports
    // Interface Properties tab widgets
    QLineEdit *m_interfacePropNameEdit;
    QGroupBox *m_dataTypeGroup;
    QLineEdit *m_dataTypeEdit;
    QPushButton *m_dataTypeButton1;
    QPushButton *m_dataTypeButton2;
    QPushButton *m_dataTypeButton3;
    QLineEdit *m_dataConstraintEdit;
    QPushButton *m_dataConstraintButton;
    QCheckBox *m_useQueuedCommCheck;
    QGroupBox *m_measurementCalibrationGroup;
    QComboBox *m_calibrationAccessCombo;
    QGroupBox *m_handleInvalidGroup;
    QButtonGroup *m_handleInvalidButtonGroup;
    QRadioButton *m_handleInvalidKeepRadio;
    QRadioButton *m_handleInvalidReplaceRadio;
    QRadioButton *m_handleInvalidNoneRadio;
    QLineEdit *m_initValueEdit;
    QComboBox *m_initValueTypeCombo;
    QPushButton *m_initValueEditButton;
    QCheckBox *m_usesTxAcknowledgeCheck;
    QLineEdit *m_timeoutEdit;
    QComboBox *m_timeoutUnitCombo;
    QCheckBox *m_usesEndToEndProtectionCheck;
    
    QPushButton *m_openButton;
    QPushButton *m_saveButton;
    QPushButton *m_saveAsButton;
    QPushButton *m_validateButton;
    QLineEdit *m_searchBox;  // Search filter box

    // State and helpers
    ArxmlModel *m_model;
    ArxmlValidator *m_validator;
    QString m_currentFileName;
    QString m_schemaFileName;
    
    // Mapping of data element names to their COM-SPEC elements (for Communication Spec tab)
    QMap<QString, std::shared_ptr<ArxmlElement>> m_dataElementToComSpec;
    
    // Get element for tree item using index path
    std::shared_ptr<ArxmlElement> getElementForItem(QTreeWidgetItem* item);
    
    // Event filter for handling clicks on Data Elements panel
    bool eventFilter(QObject *obj, QEvent *event) override;
};

#endif // MAIN_WINDOW_HPP
