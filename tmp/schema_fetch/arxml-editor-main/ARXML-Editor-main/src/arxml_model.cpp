// arxml_model.cpp
//
// SAX-based ARXML parser implementation using QXmlStreamReader for efficient
// parsing of large documents.

#include "arxml_model.hpp"

#include <QFile>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QList>

ArxmlModel::ArxmlModel()
    : m_root(std::make_shared<ArxmlElement>())
{
}

ArxmlModel::~ArxmlModel() = default;

bool ArxmlModel::loadFromFile(const QString &fileName)
{
    m_lastError.clear();
    
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_lastError = QString("Cannot open file: %1").arg(fileName);
        return false;
    }

    // Reset root
    m_root = std::make_shared<ArxmlElement>();
    m_root->tagName = "Document";
    
    QXmlStreamReader reader(&file);
    std::vector<std::shared_ptr<ArxmlElement>> stack;
    stack.push_back(m_root);

    while (!reader.atEnd()) {
        QXmlStreamReader::TokenType type = reader.readNext();

        switch (type) {
        case QXmlStreamReader::StartElement: {
            auto newElement = std::make_shared<ArxmlElement>();
            newElement->tagName = reader.name().toString();
            newElement->parent = stack.back().get();

            // Read attributes
            QXmlStreamAttributes attrs = reader.attributes();
            for (const auto& attr : attrs) {
                newElement->attributes.push_back({
                    attr.name().toString(),
                    attr.value().toString()
                });
            }

            stack.back()->children.push_back(newElement);
            stack.push_back(newElement);
            break;
        }

        case QXmlStreamReader::EndElement:
            if (!stack.empty()) {
                stack.pop_back();
            }
            break;

        case QXmlStreamReader::Characters:
            if (!reader.isWhitespace() && !stack.empty()) {
                QString text = reader.text().toString();
                if (!text.trimmed().isEmpty()) {
                    stack.back()->text = text;
                }
            }
            break;

        default:
            break;
        }
    }

    file.close();

    if (reader.hasError()) {
        m_lastError = reader.errorString();
        return false;
    }

    // Move root element's children to be root-level
    if (!m_root->children.empty()) {
        auto firstChild = m_root->children[0];
        if (firstChild->children.empty() && firstChild->text.isEmpty() &&
            !firstChild->attributes.empty()) {
            // This is likely the root element with namespace declarations
            m_root = firstChild;
        } else if (m_root->children.size() == 1) {
            // Single root element
            m_root = m_root->children[0];
            m_root->parent = nullptr;
        }
    }

    m_filePath = fileName;
    return true;
}

bool ArxmlModel::saveToFile(const QString &fileName) const
{
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QXmlStreamWriter writer(&file);
    writer.setAutoFormatting(true);
    writer.setAutoFormattingIndent(2);
    writer.writeStartDocument();

    if (m_root) {
        saveElement(*m_root, writer, 0);
    }

    writer.writeEndDocument();
    file.close();
    return true;
}

void ArxmlModel::saveElement(const ArxmlElement& elem, QXmlStreamWriter& writer, int indent) const
{
    writer.writeStartElement(elem.tagName);

    // Write attributes
    for (const auto& attr : elem.attributes) {
        writer.writeAttribute(attr.first, attr.second);
    }

    // Write text content
    if (!elem.text.isEmpty() && elem.children.empty()) {
        writer.writeCharacters(elem.text);
    }

    // Write child elements
    for (const auto& child : elem.children) {
        saveElement(*child, writer, indent + 1);
    }

    writer.writeEndElement();
}

std::shared_ptr<ArxmlElement> ArxmlModel::findElementByIndexPath(const QList<int>& indexPath) const
{
    if (indexPath.isEmpty() || !m_root) {
        return m_root; // Return root for empty path
    }
    
    return findElementByIndexPathRecursive(m_root, indexPath, 0);
}

std::shared_ptr<ArxmlElement> ArxmlModel::findElementByIndexPathRecursive(
    const std::shared_ptr<ArxmlElement>& elem,
    const QList<int>& indexPath,
    int depth) const
{
    if (!elem || depth >= indexPath.size()) {
        return elem; // Return current element if we've reached the end
    }
    
    int childIndex = indexPath[depth];
    if (childIndex < 0 || childIndex >= static_cast<int>(elem->children.size())) {
        return nullptr;
    }
    
    return findElementByIndexPathRecursive(elem->children[childIndex], indexPath, depth + 1);
}

QList<int> ArxmlModel::getElementIndexPath(const ArxmlElement* elem) const
{
    QList<int> indexPath;
    if (!elem || !m_root || elem == m_root.get()) {
        return indexPath; // Empty path for root
    }
    
    // Build path by traversing up the parent chain
    const ArxmlElement* current = elem;
    
    while (current && current->parent && current != m_root.get()) {
        int index = findChildIndex(current->parent, current);
        if (index >= 0) {
            indexPath.prepend(index);
        }
        current = current->parent;
    }
    
    return indexPath;
}

int ArxmlModel::findChildIndex(const ArxmlElement* parent, const ArxmlElement* child) const
{
    if (!parent || !child) {
        return -1;
    }
    
    for (size_t i = 0; i < parent->children.size(); ++i) {
        if (parent->children[i].get() == child) {
            return static_cast<int>(i);
        }
    }
    
    return -1;
}
