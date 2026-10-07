#pragma once

#include <QString>
#include <QVariantMap>

/**
 * @brief Represents a music genre with aggregated information.
 */
struct Genre {
    int id = 0;
    QString name;
    int songCount = 0;

    QVariantMap toVariantMap() const;
    static Genre fromVariantMap(const QVariantMap &map);
};
