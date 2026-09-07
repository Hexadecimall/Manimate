#include "AudioPlayer.h"

#include <QAudioOutput>
#include <QDir>
#include <QFileInfo>
#include <QMediaPlayer>
#include <QUrl>
#include <QtMath>

namespace mn::ui {
namespace {

/// Manim's gain is in decibels; a player wants a linear multiplier.
double linearVolume(double decibels)
{
    return qBound(0.0, std::pow(10.0, decibels / 20.0), 1.0);
}

} // namespace

AudioPlayer::AudioPlayer(QObject *parent)
    : QObject(parent)
{
}

AudioPlayer::~AudioPlayer()
{
    stop();
}

void AudioPlayer::setLayout(const ProjectLayout &layout)
{
    if (m_layout.root == layout.root)
        return;

    // A different project means different files; nothing here still applies.
    stop();
    qDeleteAll(m_players);
    m_players.clear();
    qDeleteAll(m_outputs);
    m_outputs.clear();

    m_layout = layout;
}

QMediaPlayer *AudioPlayer::playerFor(const AudioClip &clip)
{
    if (QMediaPlayer *existing = m_players.value(clip.id, nullptr))
        return existing;

    const QString path = QDir(m_layout.assetsDir).filePath(clip.asset);
    if (!QFileInfo(path).isFile())
        return nullptr;

    auto *output = new QAudioOutput(this);
    auto *player = new QMediaPlayer(this);
    player->setAudioOutput(output);
    player->setSource(QUrl::fromLocalFile(path));

    m_outputs.insert(clip.id, output);
    m_players.insert(clip.id, player);
    return player;
}

void AudioPlayer::syncTo(const Timeline &timeline, double seconds)
{
    for (const AudioClip &clip : timeline.audio) {
        QMediaPlayer *player = playerFor(clip);
        if (!player)
            continue;

        if (QAudioOutput *output = m_outputs.value(clip.id, nullptr))
            output->setVolume(float(linearVolume(clip.gain)));

        const double into = seconds - clip.start;

        // Before it starts, or past its end: silent. A sound whose length is
        // not known yet is treated as still running, since the player stops
        // itself when it reaches the end.
        const double length = player->duration() > 0 ? player->duration() / 1000.0 : -1.0;
        const bool within = into >= 0.0 && (length < 0.0 || into < length);

        if (!m_playing || !within) {
            if (player->playbackState() != QMediaPlayer::StoppedState)
                player->stop();
            continue;
        }

        player->setPosition(qint64(into * 1000.0));
        if (player->playbackState() != QMediaPlayer::PlayingState)
            player->play();
    }
}

void AudioPlayer::setPlaying(bool playing, const Timeline &timeline, double seconds)
{
    m_playing = playing;
    syncTo(timeline, seconds);
}

void AudioPlayer::seek(const Timeline &timeline, double seconds)
{
    // Scrubbing while stopped stays silent; only a running playhead makes
    // sound, or dragging it would fire off a burst of fragments.
    if (!m_playing)
        return;
    syncTo(timeline, seconds);
}

void AudioPlayer::stop()
{
    m_playing = false;
    for (QMediaPlayer *player : std::as_const(m_players)) {
        if (player->playbackState() != QMediaPlayer::StoppedState)
            player->stop();
    }
}

} // namespace mn::ui
