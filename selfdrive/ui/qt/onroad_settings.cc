#include "selfdrive/ui/qt/onroad_settings.h"

#include <utility>

#include <QApplication>
#include <QDebug>

#include "common/util.h"
#include "selfdrive/ui/qt/widgets/scrollview.h"

OnroadSettings::OnroadSettings(bool closeable, QWidget *parent) : QFrame(parent) {
  setContentsMargins(0, 0, 0, 0);
  setAttribute(Qt::WA_NoMousePropagation);

  params = Params();

  auto *frame = new QVBoxLayout(this);
  frame->setContentsMargins(40, 40, 40, 25);
  frame->setSpacing(0);

  auto *heading_frame = new QHBoxLayout;
  heading_frame->setContentsMargins(0, 0, 0, 0);
  heading_frame->setSpacing(32);
  {
    if (closeable) {
      auto *close_btn = new QPushButton("←");
      close_btn->setStyleSheet(R"(
        QPushButton {
          color: #FFFFFF;
          font-size: 100px;
          padding-bottom: 8px;
          border 1px grey solid;
          border-radius: 70px;
          background-color: #292929;
          font-weight: 500;
        }
        QPushButton:pressed {
          background-color: #3B3B3B;
        }
      )");
      close_btn->setFixedSize(140, 140);
      QObject::connect(close_btn, &QPushButton::clicked, [=]() { emit closeSettings(); });
      // TODO: read map_on_left from ui state
      heading_frame->addWidget(close_btn);
    }

    auto *heading = new QVBoxLayout;
    heading->setContentsMargins(0, 0, 0, 0);
    heading->setSpacing(16);
    {
      auto *title = new QLabel(tr("ONROAD OPTIONS"), this);
      title->setStyleSheet("color: #FFFFFF; font-size: 54px; font-weight: 600;");
      heading->addWidget(title);
    }
    heading_frame->addLayout(heading, 1);
  }
  frame->addLayout(heading_frame);
  frame->addSpacing(32);

  QWidget *options_container = new QWidget(this);
  options_layout = new QVBoxLayout(options_container);
  options_layout->setContentsMargins(0, 32, 0, 32);
  options_layout->setSpacing(20);

  // Dynamic Lane Profile
  options_layout->addWidget(dlp_widget = new OptionWidget(this));
  QObject::connect(dlp_widget, &OptionWidget::updateParam, this, &OnroadSettings::changeDynamicLaneProfile);

  // Gap Adjust Cruise
  options_layout->addWidget(gac_widget = new OptionWidget(this));
  QObject::connect(gac_widget, &OptionWidget::updateParam, this, &OnroadSettings::changeGapAdjustCruise);

  // Speed Limit Control
  options_layout->addWidget(slc_widget = new OptionWidget(this));
  QObject::connect(slc_widget, &OptionWidget::updateParam, this, &OnroadSettings::changeSpeedLimitControl);

  options_layout->addStretch();

  ScrollView *options_scroller = new ScrollView(options_container, this);
  options_scroller->setFrameShape(QFrame::NoFrame);
  frame->addWidget(options_scroller);

  auto *subtitle_frame = new QHBoxLayout;
  subtitle_frame->setContentsMargins(0, 0, 0, 0);
  {
    auto *subtitle_heading = new QVBoxLayout;
    subtitle_heading->setContentsMargins(0, 0, 0, 0);
    {
      auto *subtitle = new QLabel(tr("SUNNYPILOT FEATURES"), this);
      subtitle->setStyleSheet("color: #A0A0A0; font-size: 34px; font-weight: 300;");
      subtitle_heading->addWidget(subtitle, 0, Qt::AlignCenter);
    }
    subtitle_frame->addLayout(subtitle_heading, 1);
  }
  frame->addSpacing(25);
  frame->addLayout(subtitle_frame);

  setStyleSheet("OnroadSettings { background-color: #333333; }");

  param_watcher = new ParamWatcher(this);

  QObject::connect(param_watcher, &ParamWatcher::paramChanged, [=](const QString &param_name, const QString &param_value) {
    refresh();
  });

  refresh();
}

void OnroadSettings::changeDynamicLaneProfile() {
  UIScene &scene = uiState()->scene;
  bool can_change = scene.dynamic_lane_profile_toggle;
  if (can_change) {
    scene.dynamic_lane_profile++;
    scene.dynamic_lane_profile = scene.dynamic_lane_profile > 2 ? 0 : scene.dynamic_lane_profile;
    params.put("DynamicLaneProfile", std::to_string(scene.dynamic_lane_profile));
  }
  refresh();
}

void OnroadSettings::changeGapAdjustCruise() {
  UIScene &scene = uiState()->scene;
  const auto cp = (*uiState()->sm)["carParams"].getCarParams();
  bool can_change = hasLongitudinalControl(cp);
  if (can_change) {
    scene.longitudinal_personality--;
    scene.longitudinal_personality = scene.longitudinal_personality < 0 ? 3 : scene.longitudinal_personality;
    params.put("LongitudinalPersonality", std::to_string(scene.longitudinal_personality));
  }
  refresh();
}

void OnroadSettings::changeSpeedLimitControl() {
  UIScene &scene = uiState()->scene;
  bool can_change = true;
  if (can_change) {
    scene.speed_limit_control_enabled = !scene.speed_limit_control_enabled;
    params.putBool("SpeedLimitControl", scene.speed_limit_control_enabled);
  }
  refresh();
}

void OnroadSettings::showEvent(QShowEvent *event) {
  refresh();
}

void OnroadSettings::refresh() {
  param_watcher->addParam("DynamicLaneProfile");
  param_watcher->addParam("DynamicLaneProfileToggle");
  param_watcher->addParam("LongitudinalPersonality");
  param_watcher->addParam("SpeedLimitControl");

  if (!isVisible()) return;

  setUpdatesEnabled(false);

  const auto cp = (*uiState()->sm)["carParams"].getCarParams();

  // Dynamic Lane Profile
  dlp_widget->updateDynamicLaneProfile("DynamicLaneProfile");
  dlp_widget->setVisible(params.getBool("DynamicLaneProfileToggle"));

  // Gap Adjust Cruise
  gac_widget->updateGapAdjustCruise("LongitudinalPersonality");
  gac_widget->setVisible(hasLongitudinalControl(cp));

  // Speed Limit Control
  slc_widget->updateSpeedLimitControl("SpeedLimitControl");
  slc_widget->setVisible(true);

  setUpdatesEnabled(true);
}

OptionWidget::OptionWidget(QWidget *parent) : QPushButton(parent) {
  setContentsMargins(0, 0, 0, 0);

  auto *frame = new QHBoxLayout(this);
  frame->setContentsMargins(32, 24, 32, 24);
  frame->setSpacing(32);

  icon = new QLabel(this);
  icon->setAlignment(Qt::AlignCenter);
  icon->setFixedSize(68, 68);
  icon->setObjectName("icon");
  frame->addWidget(icon);

  auto *inner_frame = new QVBoxLayout;
  inner_frame->setContentsMargins(0, 0, 0, 0);
  inner_frame->setSpacing(0);
  {
    title = new ElidedLabel(this);
    title->setAttribute(Qt::WA_TransparentForMouseEvents);
    inner_frame->addWidget(title);

    subtitle = new ElidedLabel(this);
    subtitle->setAttribute(Qt::WA_TransparentForMouseEvents);
    subtitle->setObjectName("subtitle");
    inner_frame->addWidget(subtitle);
  }
  frame->addLayout(inner_frame, 1);

  setFixedHeight(164);
  setStyleSheet(R"(
    OptionWidget { background-color: #202123; border-radius: 10px; }
    QLabel { color: #FFFFFF; font-size: 48px; font-weight: 400; }
    #icon { background-color: #3B4356; border-radius: 34px; }
    #subtitle { color: #9BA0A5; }

    /* pressed */
    OptionWidget:pressed { background-color: #18191B; }
  )");
  QObject::connect(this, &QPushButton::clicked, [this]() { emit updateParam(); });
}

void OptionWidget::updateDynamicLaneProfile(QString param) {
  auto icon_color = "#3B4356";
  auto title_text = "";
  auto subtitle_text = "Dynamic Lane Profile";
  auto dlp = atoi(params.get(param.toStdString()).c_str());

  if (dlp == 0) {
    title_text = "Laneful";
    icon_color = "#2020f8";
  }
  else if (dlp == 1) {
    title_text = "Laneless";
    icon_color = "#0df87a";
  }
  else if (dlp == 2) {
    title_text = "Auto";
    icon_color = "#0df8f8";
  }

  icon->setStyleSheet(QString("QLabel#icon { background-color: %1; border-radius: 34px; }").arg(icon_color));

  title->setText(title_text);
  subtitle->setText(subtitle_text);
  subtitle->setVisible(true);

  setStyleSheet(styleSheet());
}

void OptionWidget::updateGapAdjustCruise(QString param) {
  auto icon_color = "#3B4356";
  auto title_text = "";
  auto subtitle_text = "Driving Personality";
  auto lp = atoi(params.get(param.toStdString()).c_str());

  if (lp == 0) {
    title_text = "Maniac Gap";
    icon_color = "#ff4b4b";
  } else if (lp == 1) {
    title_text = "Aggressive Gap";
    icon_color = "#fcff4b";
  } else if (lp == 2) {
    title_text = "Stock Gap";
    icon_color = "#4bff66";
  } else if (lp == 3) {
    title_text = "Relax Gap";
    icon_color = "#6a0ac9";
  }

  icon->setStyleSheet(QString("QLabel#icon { background-color: %1; border-radius: 34px; }").arg(icon_color));

  title->setText(title_text);
  subtitle->setText(subtitle_text);
  subtitle->setVisible(true);

  setStyleSheet(styleSheet());
}

void OptionWidget::updateSpeedLimitControl(QString param) {
  auto icon_color = "#3B4356";
  auto title_text = "";
  auto subtitle_text = "Speed Limit Conrol";
  auto speed_limit_control = atoi(params.get(param.toStdString()).c_str());

  if (speed_limit_control == 0) {
    title_text = "Disabled";
    icon_color = "#3B4356";
  } else if (speed_limit_control == 1) {
    title_text = "Enabled";
    icon_color = "#0df87a";
  }

  icon->setStyleSheet(QString("QLabel#icon { background-color: %1; border-radius: 34px; }").arg(icon_color));

  title->setText(title_text);
  subtitle->setText(subtitle_text);
  subtitle->setVisible(true);

  setStyleSheet(styleSheet());
}