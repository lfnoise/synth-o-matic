import AVFoundation
import AppKit

/// Plays sample variables.  Like the original, one sound plays at a time and
/// the keyboard transposes: each row of keys is an octave (the shifted number
/// row is four octaves up, the shifted bottom row three down), each key in a
/// row a step of the just scale 1, 9/8, 5/4, 4/3, 3/2, 5/3, 15/8, 2.
final class AudioPlayer {
    static let shared = AudioPlayer()

    static let didChange = Notification.Name("AudioPlayerDidChange")

    /// the original's key layout, from PlaySound.c
    static let keyString = Array("!@#$%^&*QWERTYUI12345678qwertyuiasdfghjkzxcvbnm,ASDFGHJKZXCVBNM<")
    static let pitchRatio: [Double] = [1.0000, 1.1250, 1.2500, 1.3333, 1.5000, 1.6667, 1.8750, 2.0000]
    static let octaveRatio: [Double] = [16.0, 8.0, 4.0, 2.0, 1.0, 0.5, 0.25, 0.125]

    /// playback rate for a key, or nil if the key doesn't play.  'a' is the original pitch.
    static func rate(forKey key: Character) -> Double? {
        guard let i = keyString.firstIndex(of: key) else { return nil }
        return pitchRatio[i & 7] * octaveRatio[i >> 3]
    }

    private let engine = AVAudioEngine()
    private let player = AVAudioPlayerNode()
    private var connectedFormat: AVAudioFormat?
    private var startTime: AVAudioTime?
    private var playingFrames: AVAudioFrameCount = 0
    private var generation = 0

    /// identifies what is playing, so views can draw a play head
    private(set) var owner: AnyObject?
    private(set) var ownerTag: String?
    private(set) var rate: Double = 1

    private init() {
        engine.attach(player)
    }

    var isPlaying: Bool { owner != nil && player.isPlaying }

    /// The position of the play head in source samples.
    var position: Double? {
        guard isPlaying, let nodeTime = player.lastRenderTime,
              let t = player.playerTime(forNodeTime: nodeTime), t.sampleTime >= 0 else { return nil }
        let frames = Double(t.sampleTime)
        return frames < Double(playingFrames) ? frames : nil
    }

    /// Plays `samples`, recorded at `sampleRate`, transposed by `rate`.  As the
    /// original did, transposing plays the samples faster or slower: the buffer
    /// claims a different sample rate and the mixer resamples it.
    func play(_ samples: [Float], sampleRate: Double, rate: Double = 1,
              normalize: Bool = true, owner: AnyObject?, tag: String?) {
        stop()
        guard !samples.isEmpty, sampleRate > 0,
              let format = AVAudioFormat(standardFormatWithSampleRate: sampleRate * max(1.0 / 64, min(64, rate)), channels: 1),
              let buffer = AVAudioPCMBuffer(pcmFormat: format, frameCapacity: AVAudioFrameCount(samples.count))
        else { return }

        var peak: Float = 0
        for s in samples where s.isFinite { peak = max(peak, abs(s)) }
        let gain: Float = normalize ? (peak > 0 ? 0.8 / peak : 0) : 1
        buffer.frameLength = AVAudioFrameCount(samples.count)
        let out = buffer.floatChannelData![0]
        for i in 0..<samples.count {
            let s = samples[i].isFinite ? samples[i] * gain : 0
            out[i] = max(-1, min(1, s))
        }

        if connectedFormat != format {
            engine.stop()
            engine.disconnectNodeOutput(player)
            engine.connect(player, to: engine.mainMixerNode, format: format)
            connectedFormat = format
        }
        do {
            if !engine.isRunning { try engine.start() }
        } catch {
            NSSound.beep()
            return
        }
        self.rate = rate
        self.owner = owner
        self.ownerTag = tag
        playingFrames = buffer.frameLength
        generation += 1
        let thisGeneration = generation
        player.scheduleBuffer(buffer, at: nil, options: []) { [weak self] in
            DispatchQueue.main.async {
                guard let self, self.generation == thisGeneration else { return }
                self.owner = nil
                self.ownerTag = nil
                NotificationCenter.default.post(name: AudioPlayer.didChange, object: self)
            }
        }
        player.play()
        NotificationCenter.default.post(name: AudioPlayer.didChange, object: self)
    }

    func stop() {
        generation += 1
        if player.isPlaying { player.stop() }
        if owner != nil {
            owner = nil
            ownerTag = nil
            NotificationCenter.default.post(name: AudioPlayer.didChange, object: self)
        }
    }
}
