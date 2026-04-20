#include "src/ui/emoji_picker.h"

#include <QDialogButtonBox>
#include <QGridLayout>
#include <QPushButton>
#include <QVBoxLayout>

namespace rpsu {

constexpr const char* EmojiPicker::kEmojis[];
constexpr int EmojiPicker::kEmojiCount;

EmojiPicker::EmojiPicker(const QString& currentEmoji, QWidget* parent)
    : QDialog(parent), selectedEmoji_(currentEmoji.isEmpty() ? QString::fromLatin1("😀") : currentEmoji) {
  setWindowTitle(QStringLiteral("Pick an Emoji"));
  setModal(true);
  resize(600, 400);
  setupUI();
}

void EmojiPicker::setupUI() {
  auto* root = new QVBoxLayout(this);
  root->setContentsMargins(12, 12, 12, 12);
  root->setSpacing(12);

  auto* gridWidget = new QWidget(this);
  auto* gridLayout = new QGridLayout(gridWidget);
  gridLayout->setSpacing(8);
  gridLayout->setContentsMargins(0, 0, 0, 0);

  constexpr int cols = 10;
  for (int i = 0; i < kEmojiCount; ++i) {
    auto* button = new QPushButton(QString::fromUtf8(kEmojis[i]), this);
    button->setFixedSize(50, 50);
    button->setStyleSheet(QStringLiteral("QPushButton { font-size: 28px; border: 1px solid #ccc; border-radius: 4px; } "
                                          "QPushButton:hover { background: #f0f0f0; }"));
    const int row = i / cols;
    const int col = i % cols;
    gridLayout->addWidget(button, row, col);

    const QString emoji = QString::fromUtf8(kEmojis[i]);
    connect(button, &QPushButton::clicked, this, [this, emoji]() {
      selectedEmoji_ = emoji;
      accept();
    });

    if (emoji == selectedEmoji_) {
      button->setStyleSheet(QStringLiteral("QPushButton { font-size: 28px; border: 2px solid #1e88e5; border-radius: 4px; background: #e3f2fd; }"));
    }
  }

  root->addWidget(gridWidget, 1);

  auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
  connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
  root->addWidget(buttonBox);
}

}  // namespace rpsu
