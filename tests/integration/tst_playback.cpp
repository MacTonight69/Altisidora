#include <QtTest>

class tst_playback : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() {}
    void cleanupTestCase() {}

    void testPlaybackWorkflow()
    {
        // TODO: Implement end-to-end playback test
        QVERIFY(true);
    }
};

QTEST_MAIN(tst_playback)
#include "tst_playback.moc"
