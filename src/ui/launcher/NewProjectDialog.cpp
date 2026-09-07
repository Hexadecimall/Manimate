#include "NewProjectDialog.h"

#include "Document.h"
#include "Theme.h"

#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QVBoxLayout>

namespace mn::ui {
namespace {

constexpr auto kLocationKey = "newProject/location";

struct ResolutionPreset
{
    const char *label;
    int width;
    int height;
};

constexpr ResolutionPreset kResolutions[] = {
    {"1920 x 1080 — Full HD", 1920, 1080},
    {"2560 x 1440 — QHD", 2560, 1440},
    {"3840 x 2160 — 4K UHD", 3840, 2160},
    {"1280 x 720 — HD", 1280, 720},
    {"1080 x 1920 — Vertical", 1080, 1920},
    {"1080 x 1080 — Square", 1080, 1080},
};

QLabel *sectionLabel(const QString &text)
{
    auto *label = new QLabel(text);
    label->setProperty("role", "section");
    return label;
}

} // namespace

QString NewProjectDialog::defaultLocation()
{
    QSettings settings;
    const QString stored = settings.value(QLatin1String(kLocationKey)).toString();
    if (!stored.isEmpty() && QDir(stored).exists())
        return stored;

    const QString movies = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    return movies.isEmpty() ? QDir::homePath() : movies;
}

void NewProjectDialog::setDefaultLocation(const QString &path)
{
    QSettings settings;
    settings.setValue(QLatin1String(kLocationKey), path);
}

NewProjectDialog::NewProjectDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("New Project"));
    setModal(true);
    setMinimumWidth(560);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(28, 24, 28, 22);
    root->setSpacing(18);

    auto *title = new QLabel(tr("New project"));
    title->setProperty("role", "title");
    root->addWidget(title);

    auto *subtitle = new QLabel(tr("Manimation creates a folder holding the project, its exported "
                                   "Python, its rendered video and its assets."));
    subtitle->setProperty("role", "subtitle");
    subtitle->setWordWrap(true);
    root->addWidget(subtitle);

    auto *form = new QFormLayout;
    form->setSpacing(12);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    m_nameEdit = new QLineEdit;
    m_nameEdit->setPlaceholderText(tr("Fourier Series"));
    form->addRow(sectionLabel(tr("NAME")), m_nameEdit);

    m_locationEdit = new QLineEdit(defaultLocation());
    auto *browseButton = new QPushButton(tr("Browse…"));
    auto *locationRow = new QHBoxLayout;
    locationRow->setSpacing(8);
    locationRow->addWidget(m_locationEdit, 1);
    locationRow->addWidget(browseButton);
    form->addRow(sectionLabel(tr("LOCATION")), locationRow);

    m_descriptionEdit = new QLineEdit;
    m_descriptionEdit->setPlaceholderText(tr("Optional — shown in the project list"));
    form->addRow(sectionLabel(tr("DESCRIPTION")), m_descriptionEdit);

    m_resolutionCombo = new QComboBox;
    for (const ResolutionPreset &preset : kResolutions)
        m_resolutionCombo->addItem(tr(preset.label), QSize(preset.width, preset.height));

    m_fpsCombo = new QComboBox;
    for (const int fps : {24, 30, 60}) {
        m_fpsCombo->addItem(tr("%1 fps").arg(fps), fps);
    }
    m_fpsCombo->setCurrentIndex(2);

    auto *formatRow = new QHBoxLayout;
    formatRow->setSpacing(8);
    formatRow->addWidget(m_resolutionCombo, 1);
    formatRow->addWidget(m_fpsCombo);
    form->addRow(sectionLabel(tr("VIDEO")), formatRow);

    root->addLayout(form);

    m_previewLabel = new QLabel;
    m_previewLabel->setProperty("role", "subtitle");
    m_previewLabel->setWordWrap(true);
    m_previewLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    root->addWidget(m_previewLabel);

    m_errorLabel = new QLabel;
    m_errorLabel->setProperty("role", "error");
    m_errorLabel->setWordWrap(true);
    m_errorLabel->hide();
    root->addWidget(m_errorLabel);

    root->addStretch(1);

    auto *cancelButton = new QPushButton(tr("Cancel"));
    m_createButton = new QPushButton(tr("Create Project"));
    m_createButton->setProperty("role", "primary");
    m_createButton->setDefault(true);
    m_createButton->setEnabled(false);

    auto *buttons = new QHBoxLayout;
    buttons->addStretch(1);
    buttons->setSpacing(10);
    buttons->addWidget(cancelButton);
    buttons->addWidget(m_createButton);
    root->addLayout(buttons);

    connect(browseButton, &QPushButton::clicked, this, &NewProjectDialog::browseForLocation);
    connect(m_nameEdit, &QLineEdit::textChanged, this, &NewProjectDialog::refreshPreview);
    connect(m_locationEdit, &QLineEdit::textChanged, this, &NewProjectDialog::refreshPreview);
    connect(m_nameEdit, &QLineEdit::returnPressed, this, &NewProjectDialog::createProject);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_createButton, &QPushButton::clicked, this, &NewProjectDialog::createProject);

    refreshPreview();
    m_nameEdit->setFocus();
}

void NewProjectDialog::browseForLocation()
{
    const QString chosen = QFileDialog::getExistingDirectory(this, tr("Choose a location"),
                                                             m_locationEdit->text());
    if (!chosen.isEmpty())
        m_locationEdit->setText(QDir::toNativeSeparators(chosen));
}

void NewProjectDialog::refreshPreview()
{
    m_errorLabel->hide();

    const QString name = project::sanitizeName(m_nameEdit->text());
    const QString location = m_locationEdit->text().trimmed();

    if (name.isEmpty() || location.isEmpty()) {
        m_previewLabel->setText(tr("Choose a name to see where the project will be created."));
        m_createButton->setEnabled(false);
        return;
    }

    const QString root = QDir(location).filePath(name);
    m_previewLabel->setText(tr("Creates %1 containing the project file, Manimation/, export/, "
                               "output/ and assets/.")
                                .arg(QDir::toNativeSeparators(root)));
    m_createButton->setEnabled(true);
}

void NewProjectDialog::createProject()
{
    if (!m_createButton->isEnabled())
        return;

    const QString location = m_locationEdit->text().trimmed();
    if (!QDir(location).exists()) {
        m_errorLabel->setText(tr("That location does not exist."));
        m_errorLabel->show();
        return;
    }

    QString error;
    ProjectLayout layout;
    if (!project::create(location, m_nameEdit->text(), &layout, &error)) {
        m_errorLabel->setText(error);
        m_errorLabel->show();
        return;
    }

    // Apply the chosen video settings to the freshly written document.
    Document document;
    if (Document::load(layout.projectFile, &document, &error)) {
        const QSize resolution = m_resolutionCombo->currentData().toSize();
        document.render.width = resolution.width();
        document.render.height = resolution.height();
        document.render.fps = m_fpsCombo->currentData().toInt();
        document.metadata.description = m_descriptionEdit->text().trimmed();
        document.metadata.revision = 0;
        if (!document.save(layout.projectFile, &error)) {
            m_errorLabel->setText(error);
            m_errorLabel->show();
            return;
        }
    }

    setDefaultLocation(location);
    m_layout = layout;
    accept();
}

} // namespace mn::ui
