#ifndef SETTINGSWIDGETS_H
#define SETTINGSWIDGETS_H

#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QSlider>
#include <QGroupBox>
#include <QToolButton>
#include <QPushButton>
#include <QRadioButton>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QPainter>
#include <QColorDialog>

#ifndef CXX_BUILD
extern "C" {
#endif

#include "../../../configuration.h"
#include "../../../setting_list.h"
#include "../../../menu/menu_setting.h"
#include "../../../menu/menu_cbs.h"

#ifndef CXX_BUILD
}
#endif

static const QString FONT_FILTER = QStringLiteral("TrueType font (*.ttf)");

class FormLayout : public QFormLayout
{
public:
   FormLayout(QWidget *parent = 0);
   void addUIntSpinBox(msg_hash_enums enum_idx);
   void addSizeSpinBox(msg_hash_enums enum_idx, unsigned scale = 1024 * 1024);
   void addFloatSpinBox(msg_hash_enums enum_idx);
   void addDirectorySelector(msg_hash_enums enum_idx);
   void addFileSelector(msg_hash_enums enum_idx);
   void addFontSelector(msg_hash_enums enum_idx);
   void addCheckBox(msg_hash_enums enum_idx);
   void addUIntComboBox(msg_hash_enums enum_idx);
   void addUIntRadioButtons(msg_hash_enums enum_idx);
   void addStringComboBox(msg_hash_enums enum_idx);
   void addStringLineEdit(msg_hash_enums enum_idx);
   void addPasswordLineEdit(msg_hash_enums enum_idx);
   void addFloatSliderAndSpinBox(msg_hash_enums enum_idx);
   void addUIntColorButton(const QString &title, msg_hash_enums r, msg_hash_enums g, msg_hash_enums b);
   bool addBindButton(msg_hash_enums enum_idx);
};

class SettingsGroup : public QGroupBox
{
   Q_OBJECT
public:
   SettingsGroup(const QString &title, QWidget *parent = 0);
   SettingsGroup(QWidget *parent = 0);
   void addWidget(QWidget *widget);
   void addRow(QString label, QWidget *widget);
   void addRow(QWidget *widget);
   void addRow(QLayout *layout);
   void addRow(QString label, QLayout *layout);
   void addCheckBox(msg_hash_enums enum_idx);
   void addFileSelector(msg_hash_enums enum_idx);
   void addDirectorySelector(msg_hash_enums enum_idx);
   void addFontSelector(msg_hash_enums enum_idx);
   void addStringLineEdit(msg_hash_enums enum_idx);
   void addPasswordLineEdit(msg_hash_enums enum_idx);
   void addStringComboBox(msg_hash_enums enum_idx);
   void addUIntSpinBox(msg_hash_enums enum_idx);
   void addUIntComboBox(msg_hash_enums enum_idx);
   void addUIntRadioButtons(msg_hash_enums enum_idx);
   void addFloatSpinBox(msg_hash_enums enum_idx);
   void addFloatSliderAndSpinBox(msg_hash_enums enum_idx);
   void addUIntColorButton(const QString &title, msg_hash_enums r, msg_hash_enums g, msg_hash_enums b);
   void addBindButton(msg_hash_enums enum_idx);
private:
   FormLayout *m_layout;
};

class CheckableSettingsGroup : public SettingsGroup
{
   Q_OBJECT
public:
   CheckableSettingsGroup(rarch_setting_t *setting, QWidget *parent = 0);
   CheckableSettingsGroup(const char *setting, QWidget *parent = 0);
   CheckableSettingsGroup(msg_hash_enums enum_idx, QWidget *parent = 0);
private slots:
   void onClicked(bool clicked);
   void paintEvent(QPaintEvent *event);
private:
   rarch_setting_t *m_setting;
   bool *m_value;
};

class CheckBox : public QCheckBox
{
   Q_OBJECT
public:
   CheckBox(rarch_setting_t *setting, QWidget *parent = 0);
   CheckBox(const char *setting, QWidget *parent = 0);
   CheckBox(msg_hash_enums enum_idx, QWidget *parent = 0);
private slots:
   void onClicked(bool checked);
   void paintEvent(QPaintEvent *event);
private:
   rarch_setting_t *m_setting;
   bool *m_value;
};

class CheckableIcon : public QToolButton
{
   Q_OBJECT
public:
   CheckableIcon(rarch_setting_t *setting, const QIcon &icon, QWidget *parent = 0);
   CheckableIcon(const char *setting, const QIcon &icon, QWidget *parent = 0);
   CheckableIcon(msg_hash_enums enum_idx, const QIcon &icon, QWidget *parent = 0);
private slots:
   void onToggled(bool checked);
   void paintEvent(QPaintEvent *event);
private:
   rarch_setting_t *m_setting;
   bool *m_value;
};

class StringLineEdit : public QLineEdit
{
   Q_OBJECT
public:
   StringLineEdit(rarch_setting_t *setting, QWidget *parent = 0);
   StringLineEdit(const char *setting, QWidget *parent = 0);
private slots:
   void onEditingFinished();
   void paintEvent(QPaintEvent *event);
private:
   rarch_setting_t *m_setting;
   char *m_value;
};

class PasswordLineEdit : public StringLineEdit
{
   Q_OBJECT
public:
   PasswordLineEdit(rarch_setting_t *setting, QWidget *parent = 0);
};

class StringComboBox : public QComboBox
{
   Q_OBJECT
public:
   StringComboBox(rarch_setting_t *setting, QWidget *parent = 0);
   StringComboBox(const char *setting, QWidget *parent = 0);
private slots:
   void onCurrentTextChanged(const QString &text);
   void paintEvent(QPaintEvent *event);
private:
   rarch_setting_t *m_setting;
   char *m_value;
};

class UIntComboBox : public QComboBox
{
   Q_OBJECT
public:
   UIntComboBox(rarch_setting_t *setting, QWidget *parent = 0);
   UIntComboBox(rarch_setting_t *setting, double min, double max, QWidget *parent = 0);
   UIntComboBox(msg_hash_enums enum_idx, QWidget *parent = 0);
   UIntComboBox(msg_hash_enums enum_idx, double min, double max, QWidget *parent = 0);
   UIntComboBox(const char *setting, QWidget *parent = 0);
private slots:
   void onCurrentIndexChanged(int index);
   void paintEvent(QPaintEvent *event);
private:
   void populate(double min, double max);
   rarch_setting_t *m_setting;
   unsigned *m_value;
   QHash<unsigned, QString> m_hash;
};

class UIntRadioButton : public QRadioButton
{
   Q_OBJECT
public:
   UIntRadioButton(const QString &text, rarch_setting_t *setting, unsigned value, QWidget *parent = 0);
   UIntRadioButton(msg_hash_enums enum_idx, unsigned value, QWidget *parent = 0);
private slots:
   void onClicked(bool);
   void paintEvent(QPaintEvent *event);
private:
   rarch_setting_t *m_setting;
   unsigned *m_target;
   unsigned m_value;
};

class UIntRadioButtons : public QGroupBox
{
   Q_OBJECT
public:
   UIntRadioButtons(rarch_setting_t *setting, QWidget *parent = 0);
   UIntRadioButtons(const char *setting, QWidget *parent = 0);
   UIntRadioButtons(msg_hash_enums enum_idx, QWidget *parent = 0);
signals:
   void currentUIntChanged(unsigned value);
private slots:
   void onButtonClicked(int id);
private:
   rarch_setting_t *m_setting;
   unsigned *m_value;
   QButtonGroup *m_buttonGroup;
};

class UIntSpinBox : public QSpinBox
{
   Q_OBJECT
public:
   UIntSpinBox(rarch_setting_t *setting, QWidget *parent = 0);
   UIntSpinBox(msg_hash_enums enum_idx, QWidget *parent = 0);
private slots:
   void onValueChanged(int value);
   void paintEvent(QPaintEvent *event);
private:
   rarch_setting_t *m_setting;
   unsigned *m_value;
};

class SizeSpinBox : public QSpinBox
{
   Q_OBJECT
public:
   SizeSpinBox(rarch_setting_t *setting, unsigned scale = 1024*1024, QWidget *parent = 0);
   SizeSpinBox(msg_hash_enums enum_idx, unsigned scale = 1024 * 1024, QWidget *parent = 0);
private slots:
   void onValueChanged(int value);
   void paintEvent(QPaintEvent *event);
private:
   rarch_setting_t *m_setting;
   size_t *m_value;
   unsigned m_scale;
};

class IntSpinBox : public QSpinBox
{
   Q_OBJECT
public:
   IntSpinBox(rarch_setting_t *setting, QWidget *parent = 0);
private slots:
   void onValueChanged(int value);
   void paintEvent(QPaintEvent *event);
private:
   rarch_setting_t *m_setting;
   int *m_value;
};

class FloatSpinBox : public QDoubleSpinBox
{
   Q_OBJECT
public:
   FloatSpinBox(rarch_setting_t *setting, QWidget *parent = 0);
   FloatSpinBox(const char *setting, QWidget *parent = 0);
   FloatSpinBox(msg_hash_enums enum_idx, QWidget *parent = 0);
private slots:
   void onValueChanged(double value);
   void paintEvent(QPaintEvent *event);
private:
   rarch_setting_t *m_setting;
   float *m_value;
   static const QRegularExpression DECIMALS_REGEX;
};

class PathButton : public QPushButton
{
   Q_OBJECT
public:
   PathButton(rarch_setting_t *setting, QWidget *parent = 0);
   PathButton(const char *setting, QWidget *parent = 0);
protected slots:
   virtual void onClicked(bool checked = false) { Q_UNUSED( checked); }
protected:
   QString m_filter;
   rarch_setting_t *m_setting;
   char *m_value;
   const char *m_dir;
};

class DirectoryButton : public PathButton
{
   Q_OBJECT
public:
   DirectoryButton(rarch_setting_t *setting, QWidget *parent = 0) :
      PathButton(setting, parent) {}
private:
   void onClicked(bool checked = false);
};

class FileButton : public PathButton
{
   Q_OBJECT
public:
   FileButton(rarch_setting_t *setting, QWidget *parent = 0) :
      PathButton(setting, parent) {}
private:
   void onClicked(bool checked = false);
};

class FontButton : public PathButton
{
   Q_OBJECT
public:
   FontButton(rarch_setting_t *setting, QWidget *parent = 0) :
      PathButton(setting, parent) {}
private:
   void onClicked(bool checked = false);
};

class DirectorySelector : public QHBoxLayout
{
   Q_OBJECT
public:
   DirectorySelector(rarch_setting_t *setting, QWidget *parent = 0);
};

class FileSelector : public QHBoxLayout
{
   Q_OBJECT
public:
   FileSelector(rarch_setting_t *setting, QWidget *parent = 0);
   FileSelector(const char *setting, QWidget *parent = 0);
};

class FontSelector : public QHBoxLayout
{
   Q_OBJECT
public:
   FontSelector(rarch_setting_t *setting, QWidget *parent = 0);
};

class FloatSlider : public QSlider
{
   Q_OBJECT
public:
   FloatSlider(rarch_setting_t *setting, QWidget *parent = 0);
   FloatSlider(const char *setting, QWidget *parent = 0);
private slots:
   void onValueChanged(int value);
   void paintEvent(QPaintEvent *event);
private:
   rarch_setting_t *m_setting;
   float *m_value;
   QRegularExpression m_decimalsRegEx;
   unsigned int m_precision;
};

class FloatSliderAndSpinBox : public QHBoxLayout
{
   Q_OBJECT
public:
   FloatSliderAndSpinBox(rarch_setting_t *setting, QWidget *parent = 0);
   FloatSliderAndSpinBox(const char *setting, QWidget *parent = 0);
   FloatSliderAndSpinBox(msg_hash_enums enum_idx, QWidget *parent = 0);
private slots:
   void onSliderValueChanged(int value);
   void onSpinBoxValueChanged(double value);
private:
   FloatSlider *m_slider;
   FloatSpinBox *m_spinBox;
};

class BindButton : public QPushButton
{
   Q_OBJECT
public:
   BindButton(rarch_setting_t *setting, QWidget *parent = 0);
   BindButton(msg_hash_enums enum_idx, QWidget *parent = 0);
private slots:
   void onClicked(bool checked);
private:
   rarch_setting_t *m_setting;
};

class ColorButton : public QToolButton
{
   Q_OBJECT
public:
   ColorButton(rarch_setting_t *red, rarch_setting_t *green, rarch_setting_t *blue, QWidget *parent = 0);
   ColorButton(msg_hash_enums red, msg_hash_enums green, msg_hash_enums blue, QWidget *parent = 0);
protected slots:
   virtual void onColorChanged(const QColor& color) { Q_UNUSED(color); }
protected:
   virtual QColor color() { return QColor(); }
   void paintEvent(QPaintEvent *event);

   rarch_setting_t *m_red, *m_green, *m_blue;
   QColorDialog *m_dialog;
};

class UIntColorButton : public ColorButton
{
   Q_OBJECT
public:
   UIntColorButton(msg_hash_enums red, msg_hash_enums green, msg_hash_enums blue, QWidget *parent = 0);
   UIntColorButton(rarch_setting_t *red, rarch_setting_t *green, rarch_setting_t *blue, QWidget *parent = 0);
protected slots:
   void onColorChanged(const QColor& color);
protected:
   QColor color();
};

class FloatColorButton : public ColorButton
{
   Q_OBJECT
public:
   explicit FloatColorButton(msg_hash_enums red, msg_hash_enums green, msg_hash_enums blue, QWidget *parent = 0);
protected slots:
   void onColorChanged(const QColor& color);
protected:
   QColor color();
};

#endif
