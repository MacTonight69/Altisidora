#pragma once

#include "MediaIntegration.h"

/**
 * @brief No-op media integration for builds without a platform backend.
 *
 * Returned by createMediaIntegration() when no backend is compiled in
 * (Windows today, or Linux with ALTISIDORA_WITH_DBUS=OFF): playback, the
 * library and the in-app controls work exactly as before; only the shell
 * integration (media keys, sound menus) is absent.
 */
class NullMediaIntegration : public MediaIntegration
{
    Q_OBJECT

public:
    explicit NullMediaIntegration(PlayerController *player, QObject *parent = nullptr);

    /**
     * @brief There is no platform media service to register with.
     * @return always false (unavailable)
     */
    bool start() override;

    /**
     * @brief Nothing was ever registered, so nothing to unregister.
     */
    void stop() override;

    /**
     * @brief Nothing is ever published.
     */
    QVariantMap metadata() const override;
};
