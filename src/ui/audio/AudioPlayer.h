#pragma once

#include "Document.h"
#include "Project.h"

#include <QHash>
#include <QObject>

class QAudioOutput;
class QMediaPlayer;

namespace mn::ui {

/// Plays the timeline's sounds while the scene plays.
///
/// Each sound gets its own player, because they overlap freely and Manim's own
/// output mixes them the same way. A player is only created once its file is
/// actually needed, so a project with sounds it never reaches costs nothing.
class AudioPlayer : public QObject
{
    Q_OBJECT

public:
    explicit AudioPlayer(QObject *parent = nullptr);
    ~AudioPlayer() override;

    /// The project the sounds belong to; their files live in its assets.
    void setLayout(const ProjectLayout &layout);

    /// Start playing from `seconds`, or stop if `playing` is false.
    void setPlaying(bool playing, const Timeline &timeline, double seconds);

    /// Move to `seconds` while playing. Ignored when stopped: scrubbing should
    /// be silent, not a stream of fragments.
    void seek(const Timeline &timeline, double seconds);

    void stop();

    bool isPlaying() const { return m_playing; }

private:
    /// The player for one clip, created on first use.
    QMediaPlayer *playerFor(const AudioClip &clip);

    /// Put every sound where `seconds` says it should be.
    void syncTo(const Timeline &timeline, double seconds);

    ProjectLayout m_layout;
    QHash<ClipId, QMediaPlayer *> m_players;
    QHash<ClipId, QAudioOutput *> m_outputs;
    bool m_playing = false;
};

} // namespace mn::ui
