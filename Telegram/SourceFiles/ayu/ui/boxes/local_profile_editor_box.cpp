// This is the source code of AyuGram for Desktop.
#include "ayu/ui/boxes/local_profile_editor_box.h"

#include "ayu/ayu_settings.h"
#include "ayu/ui/boxes/local_collectible_box.h"
#include "lang/lang_keys.h"
#include "ui/layers/generic_box.h"
#include "ui/vertical_list.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/discrete_sliders.h"
#include "ui/wrap/vertical_layout.h"
#include "ui/wrap/slide_wrap.h"
#include "data/data_peer.h"
#include "data/data_user.h"
#include "main/main_session.h"
#include "window/window_session_controller.h"
#include "styles/style_layers.h"
#include "styles/style_credits.h"

namespace {

void FillLocalProfileEditor(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionController*> controller) {
	const auto &settings = AyuSettings::getInstance();
	box->setTitle(rpl::single(QString(u"Локальная кастомизация"_q)));
	box->setWidth(420);

	const auto layout = box->verticalLayout();

	const auto tabs = layout->add(object_ptr<Ui::SettingsSlider>(
		layout,
		st::defaultSettingsSlider));
	tabs->resize(tabs->width(), st::defaultSettingsSlider.height);

	const auto tabsTexts = QStringList()
		<< QString::fromUtf8("Username")
		<< QString::fromUtf8("Phone")
		<< QString::fromUtf8("Gifts")
		<< QString::fromUtf8("Profile");
	for (const auto &text : tabsTexts) {
		tabs->addSection(text);
	}

	const auto usernameWrap = layout->add(
		object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
			layout,
			object_ptr<Ui::VerticalLayout>(layout)));
	const auto usernameLayout = usernameWrap->entity();
	const auto localUsername = usernameLayout->add(
		object_ptr<Ui::InputField>(
			usernameLayout,
			st::defaultInputField,
			rpl::single(QString(u"Локальный Username"_q)),
			settings.localProfileUsername()));
	const auto usernamePrice = usernameLayout->add(
		object_ptr<Ui::InputField>(
			usernameLayout,
			st::defaultInputField,
			rpl::single(QString(u"Цена (TON, число)"_q)),
			settings.localUsernamePrice()));
	const auto usernameDate = usernameLayout->add(
		object_ptr<Ui::InputField>(
			usernameLayout,
			st::defaultInputField,
			rpl::single(QString(u"Дата покупки (Unix timestamp)"_q)),
			settings.localUsernameDate()));
	const auto additionalUsernames = usernameLayout->add(
		object_ptr<Ui::InputField>(
			usernameLayout,
			st::defaultInputField,
			rpl::single(QString(u"Доп. Username (name=цена=дата, через запятую)"_q)),
			settings.localAdditionalUsernames()));

	Ui::AddSkip(usernameLayout);
	const auto phoneWrap = layout->add(
		object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
			layout,
			object_ptr<Ui::VerticalLayout>(layout)));
	const auto phoneLayout = phoneWrap->entity();
	const auto localPhone = phoneLayout->add(
		object_ptr<Ui::InputField>(
			phoneLayout,
			st::defaultInputField,
			rpl::single(QString(u"Локальный номер (+7... или +888...)"_q)),
			settings.localProfilePhone()));
	const auto phonePrice = phoneLayout->add(
		object_ptr<Ui::InputField>(
			phoneLayout,
			st::defaultInputField,
			rpl::single(QString(u"Цена (TON, число)"_q)),
			settings.localPhonePrice()));
	const auto phoneDate = phoneLayout->add(
		object_ptr<Ui::InputField>(
			phoneLayout,
			st::defaultInputField,
			rpl::single(QString(u"Дата покупки (Unix timestamp)"_q)),
			settings.localPhoneDate()));

	Ui::AddSkip(phoneLayout);
	const auto giftsWrap = layout->add(
		object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
			layout,
			object_ptr<Ui::VerticalLayout>(layout)));
	const auto giftsLayout = giftsWrap->entity();
	const auto giftsLabel = giftsLayout->add(
		object_ptr<Ui::FlatLabel>(
			giftsLayout,
			rpl::single(QString(u"Подарки добавляются при покупке через Telegram."_q)),
			st::defaultSubsectionTitle),
		st::defaultSubsectionTitlePadding);
	const auto giftEntries = giftsLayout->add(
		object_ptr<Ui::FlatLabel>(
			giftsLayout,
			settings.localGiftIdsValue() | rpl::map([](const QString &raw) {
				const auto records = AyuSettings::getInstance().localGiftRecords();
				if (records.empty()) {
					return QString(u"Нет добавленных подарков."_q);
				}
				auto lines = QStringList();
				for (const auto &r : records) {
					const auto title = r.title.isEmpty()
						? (u"#%1"_q.arg(r.id))
						: r.title;
					lines.append(u"%1 — %2 ⭐"_q.arg(title).arg(r.price));
				}
				return lines.join('\n');
			}),
			st::defaultSubsectionTitle),
		st::defaultSubsectionTitlePadding);

	Ui::AddSkip(giftsLayout);
	const auto giftsList = giftsLayout->add(
		object_ptr<Ui::VerticalLayout>(giftsLayout));
	const auto rebuildGiftsFn =
		box->lifetime().make_state<std::function<void()>>();
	auto rebuildGifts = [=] {
		giftsList->clear();
		const auto records = AyuSettings::getInstance().localGiftRecords();
		for (const auto &r : records) {
			const auto id = r.id;
			const auto title = r.title.isEmpty()
				? (u"#%1"_q.arg(r.id))
				: r.title;
			giftsList->add(object_ptr<Ui::FlatLabel>(
				giftsList,
				rpl::single(u"%1 — %2 ₽"_q.arg(title).arg(r.price)),
				st::defaultSubsectionTitle),
				st::defaultSubsectionTitlePadding);
			const auto edit = giftsList->add(
				object_ptr<Ui::LinkButton>(
					giftsList,
					u"Изменить модель / фон"_q));
			static_cast<Ui::LinkButton *>(edit)
				->setClickedCallback([=] {
					ShowEditLocalGiftAttributes(
						controller,
						id,
						[=] { (*rebuildGiftsFn)(); });
				});
			const auto del = giftsList->add(
				object_ptr<Ui::LinkButton>(
					giftsList,
					u"Удалить из профиля"_q));
			static_cast<Ui::LinkButton *>(del)
				->setClickedCallback([=] {
					auto &settings = AyuSettings::getInstance();
					if (settings.localProfileBackgroundGiftId() == id) {
						settings.setLocalProfileBackgroundGiftId(0);
					}
					settings.removeLocalGift(id);
					(*rebuildGiftsFn)();
				});
		}
		if (records.empty()) {
			giftsList->add(object_ptr<Ui::FlatLabel>(
				giftsList,
				rpl::single(QString(u"Нет подарков."_q)),
				st::defaultSubsectionTitle),
				st::defaultSubsectionTitlePadding);
		}
		giftsList->update();
	};
	(*rebuildGiftsFn) = rebuildGifts;
	(*rebuildGiftsFn)();
	const auto profileWrap = layout->add(
		object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
			layout,
			object_ptr<Ui::VerticalLayout>(layout)));
	const auto profileLayout = profileWrap->entity();
	const auto profileName = profileLayout->add(
		object_ptr<Ui::InputField>(
			profileLayout,
			st::defaultInputField,
			rpl::single(QString(u"Имя"_q)),
			settings.localProfileName()));
	profileLayout->add(object_ptr<Ui::FlatLabel>(
		profileLayout,
		settings.localStarsBalanceValue() | rpl::map([](int64 value) {
			return u"⭐ Локальный баланс звёзд: %1 (бесконечный)"_q.arg(value);
		}),
		st::defaultSubsectionTitle), st::defaultSubsectionTitlePadding);

	const auto wraps = std::vector<Ui::SlideWrap<Ui::VerticalLayout>*>{
		usernameWrap,
		phoneWrap,
		giftsWrap,
		profileWrap,
	};

	const auto activate = [=](int index) {
		for (auto i = 0, c = int(wraps.size()); i < c; ++i) {
			wraps[i]->toggle(i == index, anim::type::instant);
		}
	};
	tabs->sectionActivated() | rpl::on_next(activate, box->lifetime());
	activate(0);

	box->addLeftButton(rpl::single(QString(u"Сбросить"_q)), [=] {
		AyuSettings::getInstance().resetLocalProfile();
		AyuSettings::getInstance().resetLocalGifts();
		profileName->setText(QString());
		localUsername->setText(QString());
		usernamePrice->setText(QString());
		usernameDate->setText(QString());
		additionalUsernames->setText(QString());
		localPhone->setText(QString());
		phonePrice->setText(QString());
		phoneDate->setText(QString());
	});
	box->addButton(tr::lng_settings_save(), [=] {
		AyuSettings::getInstance().setLocalProfileName(profileName->getLastText().trimmed());
		AyuSettings::getInstance().setLocalProfileUsername(localUsername->getLastText().trimmed());
		AyuSettings::getInstance().setLocalUsernamePrice(usernamePrice->getLastText().trimmed());
		AyuSettings::getInstance().setLocalUsernameDate(usernameDate->getLastText().trimmed());
		AyuSettings::getInstance().setLocalAdditionalUsernames(additionalUsernames->getLastText().trimmed());
		AyuSettings::getInstance().setLocalProfilePhone(localPhone->getLastText().trimmed());
		AyuSettings::getInstance().setLocalPhonePrice(phonePrice->getLastText().trimmed());
		AyuSettings::getInstance().setLocalPhoneDate(phoneDate->getLastText().trimmed());
		box->closeBox();
	});
	box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
}

} // namespace

void ShowLocalProfileEditor(not_null<Window::SessionController*> controller) {
	controller->show(Box(FillLocalProfileEditor, controller));
}
