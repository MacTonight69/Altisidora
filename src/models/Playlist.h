#pragma once

#include <QString>
#include <QDateTime>
#include <QMetaType>
#include <QDebug>

class Playlist
{
public:
    Playlist() = default;

    Playlist(qint64 id, const QString &name, const QString &description,
             const QDateTime &dateCreated, int songCount)
        : id(id), name(name), description(description),
          dateCreated(dateCreated), songCount(songCount) {}

    qint64 id = 0;
    QString name;
    QString description;
    QDateTime dateCreated;
    int songCount = 0;

    bool isValid() const { return id > 0 && !name.isEmpty(); }
};

Q_DECLARE_METATYPE(Playlist)

inline QDebug operator<<(QDebug debug, const Playlist &playlist)
{
    QDebugStateSaver saver(debug);
    debug.nospace() << "Playlist(id=" << playlist.id
                    << ", name=" << playlist.name
                    << ", songs=" << playlist.songCount << ")";
    return debug;
}
