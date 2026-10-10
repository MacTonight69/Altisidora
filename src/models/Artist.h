#pragma once

#include <QString>

/**
 * @brief Represents a music artist with aggregated information.
 */
struct Artist {
    int id = 0;
    QString name;
    int songCount = 0;
    int albumCount = 0;
};
