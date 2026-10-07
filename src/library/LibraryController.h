#pragma once

#include <QObject>
#include <QVector>
#include <QVariant>

#include "../models/Song.h"
#include "../models/Artist.h"
#include "../models/Album.h"
#include "../models/Genre.h"
#include "../models/Playlist.h"
#include "../database/DatabaseManager.h"
#include "Scanner.h"
#include "MetadataReader.h"

/**
 * @brief Application-level library controller.
 *
 * Coordinates scanning, metadata extraction and database persistence.
 * QML interacts with this class only.
 */
class LibraryController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QVector<QVariant> songs READ songs NOTIFY libraryChanged)
    Q_PROPERTY(QVector<QVariant> albums READ albums NOTIFY libraryChanged)
    Q_PROPERTY(QVector<QVariant> artists READ artists NOTIFY libraryChanged)
    Q_PROPERTY(QVector<QVariant> genres READ genres NOTIFY libraryChanged)

public:
    explicit LibraryController(QObject *parent = nullptr);

    QVector<QVariant> songs() const;
    QVector<QVariant> albums() const;
    QVector<QVariant> artists() const;
    QVector<QVariant> genres() const;

public slots:
    void scan(const QString &directory);
    void refresh();
    QVector<QVariant> search(const QString &query) const;
    int songCount() const;

signals:
    void scanStarted();
    void scanFinished();
    void libraryChanged();

private:
    QVariantMap songToVariant(const Song &song) const;
    QVariantMap albumToVariant(const Album &album) const;
    QVariantMap artistToVariant(const Artist &artist) const;
    QVariantMap genreToVariant(const Genre &genre) const;

    DatabaseManager m_db;
    Scanner m_scanner;
    MetadataReader m_metadataReader;
    QVector<Song> m_songs;
};
