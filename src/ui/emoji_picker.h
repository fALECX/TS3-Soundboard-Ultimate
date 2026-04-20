#pragma once

#include <QString>
#include <QDialog>

class QGridLayout;

namespace rpsu {

class EmojiPicker : public QDialog {
  Q_OBJECT

 public:
  explicit EmojiPicker(const QString& currentEmoji = QString(), QWidget* parent = nullptr);
  QString selectedEmoji() const { return selectedEmoji_; }

 private:
  void setupUI();

  QString selectedEmoji_;
  static constexpr const char* kEmojis[] = {
    "😀", "😂", "😍", "🤔", "😎", "🚀", "🎉", "❤️",
    "👍", "👏", "🎵", "🎤", "🎸", "🎹", "🥁", "🎺",
    "🎷", "🎼", "🎧", "📻", "📻", "📢", "📣", "🔊",
    "🔥", "⚡", "💥", "✨", "⭐", "🌟", "💫", "🌠",
    "🎮", "🕹️", "👾", "🎯", "🎲", "🃏", "🎰", "🏆",
    "🥇", "🥈", "🥉", "🏅", "⚽", "🏀", "🎾", "🏐",
    "🏈", "🎳", "🏏", "🏑", "🏒", "🥊", "🥋", "🤺",
    "🐶", "🐱", "🐭", "🐹", "🐰", "🦊", "🐻", "🐼",
    "🐨", "🐯", "🦁", "🐮", "🐷", "🐸", "🐵", "🙈",
    "🙉", "🙊", "🐒", "🐔", "🐧", "🐦", "🐤", "🦆",
    "🦅", "🦉", "🦇", "🐺", "🐗", "🐴", "🦄", "🐝",
    "🐛", "🦋", "🐌", "🐞", "🐜", "🪰", "🪲", "🦟",
    "🍕", "🍔", "🍟", "🌭", "🍿", "🥓", "🥚", "🍳",
    "🧈", "🍞", "🥐", "🥖", "🥨", "🥯", "🧀", "🍗",
    "🍖", "🌮", "🌯", "🫔", "🥙", "🧆", "🌲", "🥗",
    "🥘", "🥫", "🍝", "🍜", "🍲", "🍛", "🍣", "🍱",
    "🥟", "🦪", "🍤", "🍙", "🍚", "🍧", "🍨", "🍦",
    "🍰", "🎂", "🧁", "🍮", "🍭", "🍬", "🍫", "🍿",
    "☕", "🍵", "🍶", "🍾", "🍷", "🍸", "🍹", "🍺",
    "🚗", "🚕", "🚙", "🚌", "🚎", "🏎️", "🚓", "🚑",
    "⚽", "🎱", "🎳", "🎮", "🎰", "🎨", "🎭", "🎪"
  };
  static constexpr int kEmojiCount = 155;
};

}  // namespace rpsu
