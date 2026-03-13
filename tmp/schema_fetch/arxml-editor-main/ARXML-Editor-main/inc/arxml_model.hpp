// arxml_model.hpp
//
// ArxmlModel with SAX-based parsing for improved performance with large files.
// Uses an internal tree structure for manipulation and QXmlStreamWriter for saving.

#ifndef ARXML_MODEL_HPP
#define ARXML_MODEL_HPP

#include <QString>
#include <QVariant>
#include <memory>
#include <vector>

class ArxmlElement
{
public:
    QString tagName;
    QString text;
    std::vector<std::pair<QString, QString>> attributes;
    std::vector<std::shared_ptr<ArxmlElement>> children;
    ArxmlElement* parent = nullptr;

    ArxmlElement() = default;
    
    std::shared_ptr<ArxmlElement> createChild(const QString& name) {
        auto child = std::make_shared<ArxmlElement>();
        child->tagName = name;
        child->parent = this;
        children.push_back(child);
        return child;
    }

    void removeChild(std::shared_ptr<ArxmlElement> child) {
        children.erase(
            std::remove_if(children.begin(), children.end(),
                [child](const std::shared_ptr<ArxmlElement>& ptr) { return ptr == child; }),
            children.end()
        );
    }

    void setAttribute(const QString& name, const QString& value) {
        for (auto& attr : attributes) {
            if (attr.first == name) {
                attr.second = value;
                return;
            }
        }
        attributes.push_back({name, value});
    }

    QString getAttribute(const QString& name) const {
        for (const auto& attr : attributes) {
            if (attr.first == name) {
                return attr.second;
            }
        }
        return QString();
    }
};

class ArxmlModel
{
public:
    ArxmlModel();
    ~ArxmlModel();

    // Load an ARXML file using SAX parser. Returns true on success.
    bool loadFromFile(const QString &fileName);

    // Save using QXmlStreamWriter. Returns true on success.
    bool saveToFile(const QString &fileName) const;

    // Access the root element
    std::shared_ptr<ArxmlElement> rootElement() const { return m_root; }

    // For validation - get the file path
    QString filePath() const { return m_filePath; }

    // Get error message if load failed
    QString lastError() const { return m_lastError; }

    // Find element by index path (list of child indices from root)
    std::shared_ptr<ArxmlElement> findElementByIndexPath(const QList<int>& indexPath) const;
    
    // Get index path to element (list of child indices from root)
    QList<int> getElementIndexPath(const ArxmlElement* elem) const;

private:
    std::shared_ptr<ArxmlElement> m_root;
    QString m_filePath;
    QString m_lastError;
    
    void saveElement(const ArxmlElement& elem, class QXmlStreamWriter& writer, int indent) const;
    std::shared_ptr<ArxmlElement> findElementByIndexPathRecursive(
        const std::shared_ptr<ArxmlElement>& elem, 
        const QList<int>& indexPath, 
        int depth) const;
    int findChildIndex(const ArxmlElement* parent, const ArxmlElement* child) const;
};

#endif // ARXML_MODEL_HPP
