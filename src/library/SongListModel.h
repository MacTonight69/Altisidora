#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QStringList>
#include <QVariantList>
#include <QVector>

#include "../models/Song.h"

/**
 * @brief List model over a snapshot of the song library.
 *
 * Owns a QVector<Song> (the same snapshots LibraryController keeps) and
 * exposes the Song fields as named roles, so QML delegates read
 * model.title / model.artist / ... instead of re-wrapping every song into
 * a QVariantMap per binding evaluation. Role-based bindings also work for
 * plain JS arrays (model.<prop> proxies to the array element), which lets
 * the search path keep using arrays until it is migrated too.
 */
class SongListModel : public QAbstractListModel
{
    Q_OBJECT
    // Row count exposed to QML: the search path needs it to size selection
    // ranges and to decide the empty/playable state without materialising a
    // QVariantList.
    Q_PROPERTY(int count READ count NOTIFY countChanged)
public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        PathRole,
        TitleRole,
        ArtistRole,
        AlbumRole,
        GenreRole,
        YearRole,
        TrackNumberRole,
        DiscNumberRole,
        DurationRole,
        ArtworkRole,
        FavoriteRole,
        PlayCountRole,
        LastPlayedRole,
    };

    explicit SongListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const;

    void setSongs(const QVector<Song> &songs);

    /**
     * @brief Rebuild the rows incrementally from a new snapshot.
     *
     * Diffs the current rows against @p songs by id and emits row-level
     * add/remove/dataChanged signals instead of a full beginResetModel(), so
     * a scan that touches a handful of songs only re-renders those rows.
     *
     * Shared ids are expected to keep their relative order (the snapshot is
     * always ordered by title, path). When they do not — e.g. a song's title
     * changed and it must move — the update falls back to a full reset,
     * because signalling a reorder row-by-row would need persistent indexes.
     */
    void applySnapshot(const QVector<Song> &songs);

    /**
     * @brief Sort the rows by a role, in place.
     *
     * Reordering is signalled as a full reset: a sort moves arbitrary rows,
     * and emitting one move at a time would require persistent model indexes
     * for no practical gain on a user-initiated, infrequent action.
     */
    void sort(Role role, Qt::SortOrder order = Qt::AscendingOrder);

    // For QML scripts that still need a map (e.g. pushing into the Player
    // queue) or a row lookup by song id.
    Q_INVOKABLE QVariantMap songData(int row) const;
    Q_INVOKABLE int rowOfId(int id) const;
    // Just the id of a row. Selection walks (select range / select all) need
    // nothing else, and building a QVariantMap per row for that both costs
    // ~2 us here and makes QML materialise a 15-property JS object per row.
    Q_INVOKABLE int idAt(int row) const;
    // Paths of the rows whose id is in @p ids, in row order. Indexes the
    // rows by id once, so it never builds a QVariantMap per scanned row the
    // way calling songData() in a loop would. Used by the multi-select hide
    // flow, which needs paths (not maps) for the whole selection.
    Q_INVOKABLE QStringList pathsForIds(const QVariantList &ids) const;

    // Whole snapshot as a QVariantList of maps. Only used to build a playback
    // queue (Player.setQueue takes maps), never for rendering rows.
    Q_INVOKABLE QVariantList toVariantList() const;

signals:
    void countChanged();

private:
    // Role -> field value as a QVariant. Single source for data() and sort().
    static QVariant fieldFor(const Song &song, Role role);

    // True when every QML-visible field matches. The file fingerprints
    // (mtimeMs/size) are internal and never rendered, so they are excluded.
    static bool sameDisplayData(const Song &a, const Song &b);

    QVector<Song> m_songs;
};