// This is the source code of AyuGram for Desktop.
#include "ayu/ui/boxes/local_collectible_box.h"

#include "ayu/ayu_settings.h"
#include "apiwrap.h"
#include "api/api_premium.h"
#include "base/random.h"
#include "base/timer.h"
#include "boxes/background_preview_box.h"
#include "boxes/share_box.h"
#include "boxes/peer_list_box.h"
#include "boxes/peer_list_controllers.h"
#include "ui/image/image_location.h"
#include "ui/boxes/confirm_box.h"
#include "history/history.h"
#include "history/history_item.h"
#include "data/data_document_media.h"
#include "settings/settings_credits_graphics.h"

#include <QGuiApplication>
#include "data/data_emoji_statuses.h"
#include "boxes/transfer_gift_box.h"
#include "boxes/star_gift_preview_box.h"
#include "boxes/star_gift_cover_box.h"
#include "rpl/rpl.h"
#include "ui/boxes/collectible_info_box.h"
#include "ui/widgets/buttons.h"
#include "ui/vertical_list.h"
#include "ui/dynamic_thumbnails.h"
#include "ui/text/text_utilities.h"
#include "window/window_session_controller.h"
#include "main/main_session.h"
#include "data/data_user.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "data/data_document.h"
#include "data/data_star_gift.h"
#include "data/data_wall_paper.h"
#include "lang/lang_keys.h"
#include "base/unixtime.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/fields/input_field.h"
#include "styles/style_layers.h"
#include "styles/style_widgets.h"

namespace {

[[nodiscard]] QString FormatDate(TimeId date) {
	return langDateTime(base::unixtime::parse(date));
}

[[nodiscard]] QString FormatTON(uint64 amount) {
	const auto major = amount / 1'000'000'000;
	const auto minor = amount % 1'000'000'000;
	if (minor == 0) {
		return u"%1 TON"_q.arg(major);
	}
	return u"%1.%2 TON"_q.arg(major).arg(minor, 9, 10, QChar('0')).trimmed();
}

constexpr auto kTonToUsdRate = 5.2;

[[nodiscard]] uint64 TonsToUsdCents(int64 tons) {
	return uint64(tons * kTonToUsdRate * 100);
}

[[nodiscard]] QString FormatNumber(int number) {
	auto result = QString::number(number);
	auto i = result.size() - 3;
	while (i > 0) {
		result.insert(i, u'\u00A0');
		i -= 3;
	}
	return result;
}

void AddInfoRow(
		Ui::VerticalLayout *layout,
		const QString &label,
		const QString &value) {
	layout->add(object_ptr<Ui::FlatLabel>(
		layout,
		rpl::single(u"%1: %2"_q.arg(label, value)),
		st::defaultSubsectionTitle),
		st::defaultSubsectionTitlePadding);
}

} // namespace

AyuSettings::LocalGiftRecord MakeLocalGiftRecord(
		const Data::StarGift &info,
		int64 price) {
	auto record = AyuSettings::LocalGiftRecord();
	record.id = info.id;
	record.price = price;
	record.date = base::unixtime::now();
	record.documentId = info.document->id;
	if (const auto &unique = info.unique) {
		record.initialGiftId = unique->initialGiftId;
		record.title = unique->title;
		record.slug = unique->slug;
		record.number = unique->number;
		record.modelName = unique->model.name;
		record.modelRarity = unique->model.rarityValue;
		record.modelDocumentId = unique->model.document->id;
		record.patternName = unique->pattern.name;
		record.patternRarity = unique->pattern.rarityValue;
		record.patternDocumentId = unique->pattern.document->id;
		record.backdropName = unique->backdrop.name;
		record.backdropRarity = unique->backdrop.rarityValue;
		record.backdropCenter = unique->backdrop.centerColor;
		record.backdropEdge = unique->backdrop.edgeColor;
		record.backdropPattern = unique->backdrop.patternColor;
		record.backdropText = unique->backdrop.textColor;
		record.backdropId = unique->backdrop.id;
		if (unique->value) {
			record.valueCurrency = unique->value->currency;
			record.valueAmount = unique->value->valuePrice;
			record.valueUsd = unique->value->valuePriceUsd;
		}
	} else {
		record.title = info.resellTitle;
		if (const auto &bg = info.background) {
			record.backdropCenter = bg->center;
			record.backdropEdge = bg->edge;
			record.backdropPattern = bg->edge;
			record.backdropText = bg->text;
		}
	}
	record.limitedLeft = info.limitedLeft;
	record.limitedCount = info.limitedCount;
	return record;
}

std::optional<Data::SavedStarGift> LocalGiftFromRecord(
		not_null<PeerData*> peer,
		const AyuSettings::LocalGiftRecord &record,
		DocumentData *fallbackDoc) {
	auto doc = static_cast<DocumentData*>(nullptr);
	if (record.documentId) {
		doc = peer->owner().document(record.documentId).get();
	}
	if (!doc) {
		doc = fallbackDoc;
	}
	if (!doc) {
		return std::nullopt;
	}
	auto modelDoc = doc;
	if (record.modelDocumentId) {
		modelDoc = peer->owner().document(record.modelDocumentId).get();
	}
	auto patternDoc = doc;
	if (record.patternDocumentId) {
		patternDoc = peer->owner().document(record.patternDocumentId).get();
	}

	const auto seed = uint32(record.id);
	const auto hasAttrs = record.hasAttributes();
	const auto center = record.backdropCenter.isValid()
		? record.backdropCenter
		: QColor::fromHsv(
			(seed * 37) % 360,
			80 + (seed % 40),
			180 + (seed % 75));
	const auto edge = record.backdropEdge.isValid()
		? record.backdropEdge
		: QColor::fromHsv(
			(seed * 73) % 360,
			80 + (seed % 40),
			140 + (seed % 75));
	const auto patternColor = record.backdropPattern.isValid()
		? record.backdropPattern
		: edge.lighter(130);
	const auto textColor = record.backdropText.isValid()
		? record.backdropText
		: QColor(255, 255, 255);

	const auto backdropNames = std::vector<QString>{
		u"Sapphire"_q, u"Ruby"_q, u"Emerald"_q,
		u"Obsidian"_q, u"Crystal"_q, u"Amber"_q,
		u"Topaz"_q, u"Amethyst"_q, u"Coral"_q,
		u"Jade"_q, u"Onyx"_q, u"Opal"_q,
	};
	const auto modelNames = std::vector<QString>{
		u"Lion"_q, u"Eagle"_q, u"Dragon"_q,
		u"Wolf"_q, u"Tiger"_q, u"Phoenix"_q,
		u"Unicorn"_q, u"Panda"_q, u"Leopard"_q,
	};
	const auto patternNames = std::vector<QString>{
		u"Flame"_q, u"Frost"_q, u"Storm"_q,
		u"Light"_q, u"Shadow"_q, u"Galaxy"_q,
		u"Neon"_q, u"Comet"_q, u"Aurora"_q,
	};
	const auto giftTitles = std::vector<QString>{
		u"Gift"_q, u"Present"_q, u"Reward"_q,
		u"Prize"_q, u"Medal"_q, u"Token"_q,
	};

	const auto modelName = hasAttrs && !record.modelName.isEmpty()
		? record.modelName
		: modelNames[seed % modelNames.size()];
	const auto patternName = hasAttrs && !record.patternName.isEmpty()
		? record.patternName
		: patternNames[seed % patternNames.size()];
	const auto backdropName = hasAttrs && !record.backdropName.isEmpty()
		? record.backdropName
		: backdropNames[seed % backdropNames.size()];
	const auto title = !record.title.isEmpty()
		? record.title
		: giftTitles[seed % giftTitles.size()];
	const auto number = record.number ? record.number : int(record.id);
	const auto modelRarity = hasAttrs ? record.modelRarity : 0;
	const auto patternRarity = hasAttrs ? record.patternRarity : 0;
	const auto backdropRarity = hasAttrs ? record.backdropRarity : 0;
	const auto backdropId = hasAttrs
		? record.backdropId
		: int(seed % 1000);
	const auto slug = record.slug;
	const auto valuePrice = record.valueAmount ? record.valueAmount : record.price;
	const auto valueCurrency = !record.valueCurrency.isEmpty()
		? record.valueCurrency
		: u"USD"_q;
	const auto valueUsd = record.valueUsd
		? record.valueUsd
		: int64(record.price * 5.2);
	const auto limitedLeft = record.limitedCount > 0
		? record.limitedLeft
		: 0;
	const auto limitedCount = record.limitedCount > 0
		? record.limitedCount
		: 1;

	auto backdropValue = std::make_shared<Data::UniqueGiftValue>();
	backdropValue->currency = valueCurrency;
	backdropValue->valuePrice = valuePrice;
	backdropValue->valuePriceUsd = valueUsd;

	auto unique = std::shared_ptr<Data::UniqueGift>(
		new Data::UniqueGift{
			CollectibleId(record.id), // id
			0,                      // initialGiftId
			slug,                   // slug
			title,                  // title
			{},                     // giftAddress
			{},                     // ownerAddress
			{},                     // ownerName
			peer->id,               // ownerId
			peer->id,               // hostId
			nullptr,                // releasedBy
			nullptr,                // themeUser
			0,                      // nanoTonForResale
			0,                      // craftChancePermille
			0,                      // starsForResale
			0,                      // starsForTransfer
			0,                      // starsMinOffer
			number,                 // number
			false,                  // onlyAcceptTon
			false,                  // canBeTheme
			false,                  // crafted
			false,                  // burned
			0,                      // exportAt
			0,                      // canTransferAt
			0,                      // canResellAt
			0,                      // canCraftAt
			{ modelName, modelRarity, modelDoc },     // model
			{ patternName, patternRarity, patternDoc }, // pattern
		});
	unique->backdrop = {
		backdropName,
		backdropRarity,
		center,
		edge,
		patternColor,
		textColor,
		backdropId,
	};
	unique->value = std::move(backdropValue);

	auto bg = std::make_shared<Data::StarGiftBackground>();
	bg->center = center;
	bg->edge = edge;
	bg->text = textColor;

	auto saved = std::shared_ptr<Data::SavedStarGift>(
		new Data::SavedStarGift{
			{
				record.id, // id
				unique, // unique
				bg, // background
				record.price, // stars
				0, // starsConverted
				0, // starsToUpgrade
				0, // starsResellMin
				modelDoc, // document
				nullptr, // releasedBy
				title, // resellTitle
				0, // resellCount
				{}, // auctionSlug
				0, // auctionGiftsPerRound
				0, // auctionStartDate
				limitedLeft, // limitedLeft
				limitedCount, // limitedCount
			},
		});
	saved->date = TimeId(record.date);
	saved->giftNum = number;
	saved->mine = peer->isSelf();
	saved->manageId = Data::SavedStarGiftId::User(MsgId(record.id));
	saved->hidden = AyuSettings::getInstance().isLocalGiftHiddenFromProfile(
		record.id);
	return std::move(*saved);
}

void SetLocalChatBackground(
		not_null<Window::SessionController*> controller,
		const AyuSettings::LocalGiftRecord &record) {
	if (!record.backdropCenter.isValid() || !record.backdropEdge.isValid()) {
		return;
	}
	controller->show(Box<BackgroundPreviewBox>(
		controller,
		Data::CustomWallPaper().withBackgroundColors({
			record.backdropCenter,
			record.backdropEdge,
			record.backdropEdge,
		})));
}

void ShowLocalGiftBox(
		not_null<Window::SessionController*> controller,
		const AyuSettings::LocalGiftRecord &record) {
	const auto peer = controller->session().user();
	auto doc = record.documentId
		? peer->owner().document(record.documentId).get()
		: nullptr;
	if (doc && doc->sticker()) {
		if (auto gift = LocalGiftFromRecord(peer, record, nullptr)) {
			::Settings::ShowSavedStarGiftBox(controller, peer, *gift);
		}
		return;
	}
	if (record.slug.isEmpty()) {
		return;
	}
	const auto session = &controller->session();
	session->api().request(
		MTPpayments_GetUniqueStarGift(MTP_string(record.slug))
	).done([=](const MTPpayments_UniqueStarGift &result) {
		session->data().processUsers(result.data().vusers());
		ShowLocalGiftBox(controller, record);
	}).send();
}

std::set<quint64> LocalTransferInjected;

MTPStarGift ApplyLocalGiftRecordToMTP(
		not_null<Main::Session*> session,
		not_null<PeerData*> to,
		const AyuSettings::LocalGiftRecord &record,
		const MTPStarGift &stock) {
	const auto &d = stock.c_starGiftUnique();
	const auto findDoc = [&](quint64 id) -> DocumentData* {
		if (!id) {
			return nullptr;
		}
		const auto result = session->data().document(id).get();
		return (result && result->sticker()) ? result : nullptr;
	};
	const auto base = findDoc(record.documentId);
	const auto modelDoc = findDoc(record.modelDocumentId)
		? findDoc(record.modelDocumentId)
		: base;
	const auto patternDoc = findDoc(record.patternDocumentId)
		? findDoc(record.patternDocumentId)
		: base;
	if (!modelDoc || !patternDoc || !record.hasAttributes()) {
		return stock;
	}
	const auto docMTP = [&](not_null<DocumentData*> doc) -> MTPDocument {
		const auto input = doc->mtpInput();
		const auto hasAccess = (input.type() == mtpc_inputDocument);
		const auto sticker = doc->sticker();
		auto docAttributes = QVector<MTPDocumentAttribute>();
		docAttributes.push_back(MTP_documentAttributeSticker(
			MTP_flags(0),
			MTP_string(sticker ? sticker->alt : QString()),
			MTP_inputStickerSetEmpty(),
			MTPMaskCoords()));
		return MTP_document(
			MTP_flags(0),
			MTP_long(int64(doc->id)),
			hasAccess
				? input.c_inputDocument().vaccess_hash()
				: MTP_long(0),
			hasAccess
				? input.c_inputDocument().vfile_reference()
				: MTP_bytes(),
			MTP_int(doc->date),
			MTP_string(doc->mimeString()),
			MTP_long(doc->size),
			MTP_vector<MTPPhotoSize>(),
			MTPVector<MTPVideoSize>(),
			MTP_int(doc->getDC()),
			MTP_vector<MTPDocumentAttribute>(std::move(docAttributes)));
	};
	const auto colorToInt = [](const QColor &color) {
		return int((quint32(color.red()) << 16)
			| (quint32(color.green()) << 8)
			| quint32(color.blue()));
	};
	auto attributes = QVector<MTPStarGiftAttribute>();
	attributes.push_back(MTP_starGiftAttributeModel(
		MTP_flags(0),
		MTP_string(!record.modelName.isEmpty()
			? record.modelName
			: u"Model"_q),
		docMTP(modelDoc),
		MTP_starGiftAttributeRarity(
			MTP_int(std::max(record.modelRarity, 0)))));
	attributes.push_back(MTP_starGiftAttributePattern(
		MTP_string(!record.patternName.isEmpty()
			? record.patternName
			: u"Pattern"_q),
		docMTP(patternDoc),
		MTP_starGiftAttributeRarity(
			MTP_int(std::max(record.patternRarity, 0)))));
	if (record.backdropCenter.isValid()
		&& record.backdropEdge.isValid()) {
		const auto patternColor = record.backdropPattern.isValid()
			? record.backdropPattern
			: record.backdropEdge;
		const auto textColor = record.backdropText.isValid()
			? record.backdropText
			: QColor(255, 255, 255);
		attributes.push_back(MTP_starGiftAttributeBackdrop(
			MTP_string(!record.backdropName.isEmpty()
				? record.backdropName
				: u"Backdrop"_q),
			MTP_int(record.backdropId),
			MTP_int(colorToInt(record.backdropCenter)),
			MTP_int(colorToInt(record.backdropEdge)),
			MTP_int(colorToInt(patternColor)),
			MTP_int(colorToInt(textColor)),
			MTP_starGiftAttributeRarity(
				MTP_int(std::max(record.backdropRarity, 0)))));
	} else {
		for (const auto &attribute : d.vattributes().v) {
			if (attribute.type() == mtpc_starGiftAttributeBackdrop) {
				attributes.push_back(attribute);
				break;
			}
		}
	}
	return MTP_starGiftUnique(
		d.vflags(),
		MTP_long(int64(record.id)),
		MTP_long(int64(record.initialGiftId
			? record.initialGiftId
			: record.id)),
		MTP_string(!record.title.isEmpty()
			? record.title
			: qs(d.vtitle())),
		d.vslug(),
		MTP_int(record.number ? record.number : d.vnum().v),
		MTP_peerUser(peerToBareMTPInt(to->id)),
		MTP_string(to->name()),
		(d.vowner_address() ? *d.vowner_address() : MTP_string(QString())),
		MTP_vector<MTPStarGiftAttribute>(std::move(attributes)),
		d.vavailability_issued(),
		d.vavailability_total(),
		(d.vgift_address() ? *d.vgift_address() : MTP_string(QString())),
		MTP_vector<MTPStarsAmount>(),
		(d.vreleased_by() ? *d.vreleased_by() : MTPPeer()),
		(d.vvalue_amount() ? *d.vvalue_amount() : MTP_long(0)),
		(d.vvalue_currency() ? *d.vvalue_currency() : MTP_string(QString())),
		(d.vvalue_usd_amount() ? *d.vvalue_usd_amount() : MTP_long(0)),
		(d.vtheme_peer() ? *d.vtheme_peer() : MTPPeer()),
		(d.vpeer_color() ? *d.vpeer_color() : MTPPeerColor()),
		(d.vhost_id() ? *d.vhost_id() : MTPPeer()),
		(d.voffer_min_stars() ? *d.voffer_min_stars() : MTP_int(0)),
		(d.vcraft_chance_permille() ? *d.vcraft_chance_permille() : MTP_int(0)));
}

void InsertTransferServiceMessage(
		not_null<Main::Session*> session,
		not_null<History*> history,
		not_null<PeerData*> to,
		const MTPStarGift &giftTL,
		MsgId msgId,
		TimeId date,
		MessageFlags localFlags) {
	const auto &d = giftTL.c_starGiftUnique();
	const auto selfId = session->userPeerId();
	const auto gift = MTP_starGiftUnique(
		d.vflags(),
		d.vid(),
		d.vgift_id(),
		d.vtitle(),
		d.vslug(),
		d.vnum(),
		MTP_peerUser(peerToBareMTPInt(to->id)),
		MTP_string(to->name()),
		(d.vowner_address() ? *d.vowner_address() : MTP_string(QString())),
		d.vattributes(),
		d.vavailability_issued(),
		d.vavailability_total(),
		(d.vgift_address() ? *d.vgift_address() : MTP_string(QString())),
		MTP_vector<MTPStarsAmount>(),
		(d.vreleased_by() ? *d.vreleased_by() : MTPPeer()),
		(d.vvalue_amount() ? *d.vvalue_amount() : MTP_long(0)),
		(d.vvalue_currency() ? *d.vvalue_currency() : MTP_string(QString())),
		(d.vvalue_usd_amount() ? *d.vvalue_usd_amount() : MTP_long(0)),
		(d.vtheme_peer() ? *d.vtheme_peer() : MTPPeer()),
		(d.vpeer_color() ? *d.vpeer_color() : MTPPeerColor()),
		(d.vhost_id() ? *d.vhost_id() : MTPPeer()),
		(d.voffer_min_stars() ? *d.voffer_min_stars() : MTP_int(0)),
		(d.vcraft_chance_permille() ? *d.vcraft_chance_permille() : MTP_int(0)));
	const auto action = MTP_messageActionStarGiftUnique(
		MTP_flags(MTPDmessageActionStarGiftUnique::Flag::f_transferred
			| MTPDmessageActionStarGiftUnique::Flag::f_from_id),
		gift,
		MTP_int(0),
		MTP_long(0),
		MTP_peerUser(peerToBareMTPInt(to->id)),
		MTP_peerUser(peerToBareMTPInt(selfId)),
		MTP_long(0),
		MTP_starsAmount(MTP_long(0), MTP_int(0)),
		MTP_int(0),
		MTP_int(0),
		MTP_long(0),
		MTP_int(0));
	const auto item = history->makeMessage(
		msgId,
		MTPDmessageService(
			MTP_flags(MTPDmessageService::Flag::f_out
				| MTPDmessageService::Flag::f_from_id),
			MTP_int(msgId.bare),
			MTP_peerUser(peerToBareMTPInt(selfId)),
			MTP_peerUser(peerToBareMTPInt(to->id)),
			MTPPeer(),
			MTPMessageReplyHeader(),
			MTP_int(date),
			action,
			MTPMessageReactions(),
			MTP_int(0)),
		localFlags);
	history->insertMessageToBlocks(item);
}

struct LocalTransferSwapState {
	std::optional<MTPStarGift> gift;
	MsgId msgId;
	AyuSettings::LocalGiftRecord record;
	bool active = true;
	rpl::lifetime lifetime;
};

void AddLocalTransferMessage(
		not_null<Main::Session*> session,
		not_null<PeerData*> to,
		const AyuSettings::LocalGiftRecord &record,
		Fn<void()> done) {
	LocalTransferInjected.insert(record.id);
	const auto slug = record.slug;
	if (slug.isEmpty()) {
		if (done) {
			done();
		}
		return;
	}
	const auto history = session->data().history(to);
	const auto state = std::make_shared<LocalTransferSwapState>();
	state->record = record;
	const auto finish = [=] {
		state->active = false;
		crl::on_main([state] { state->lifetime.destroy(); });
		if (done) {
			done();
		}
	};
	const auto trySwap = [=] {
		if (!state->gift || !state->msgId || !state->active) {
			return;
		}
		const auto swapMsgId = state->msgId;
		crl::on_main([=] {
			if (!state->active) {
				return;
			}
			state->active = false;
			const auto dot = session->data().message(to->id, swapMsgId);
			const auto date = dot
				? dot->date()
				: base::unixtime::now();
			if (dot) {
				history->destroyMessage(dot);
			}
			InsertTransferServiceMessage(
				session,
				history,
				to,
				*state->gift,
				swapMsgId,
				date,
				MessageFlags());
			AyuSettings::getInstance().setLocalTransferMsgId(
				state->record.id,
				swapMsgId.bare);
			LOG((u"AyuGiftsMsg: swapped dot %1 for transfer %2"_q
				.arg(swapMsgId.bare)
				.arg(state->record.id)));
			finish();
		});
	};
	session->api().request(
		MTPpayments_GetUniqueStarGift(MTP_string(slug))
	).done([=](const MTPpayments_UniqueStarGift &result) {
		session->data().processUsers(result.data().vusers());
		if (const auto parsed = Api::FromTL(session, result.data().vgift())) {
			if (parsed->document) {
				parsed->document->createMediaView()->checkStickerLarge();
			}
		}
		state->gift = ApplyLocalGiftRecordToMTP(
			session,
			to,
			record,
			result.data().vgift());
		trySwap();
	}).fail([=](const MTP::Error &) {
		finish();
	}).send();
	auto action = Api::SendAction(history);
	action.clearDraft = false;
	auto message = Api::MessageToSend(std::move(action));
	message.textWithTags.text = u"."_q;
	session->api().sendMessage(std::move(message));
	const auto dot = history->lastMessage();
	if (!dot || !dot->out() || !dot->isSending()) {
		LOG((u"AyuGiftsMsg: dot item missing after send for %1"_q).arg(record.id));
		return;
	}
	const auto localId = dot->id;
	session->data().itemIdChanged(
	) | rpl::on_next([=](Data::Session::IdChange change) {
		if (!state->active
			|| change.newId.peer != to->id
			|| change.oldId != localId) {
			return;
		}
		state->msgId = change.newId.msg;
		trySwap();
	}, state->lifetime);
}
constexpr auto kTransferRestoreAttemptInterval = crl::time(200);
constexpr auto kTransferRestoreAttemptBudget = 100;

bool EnsureLocalTransferMessages(
		not_null<PeerData*> to,
		Fn<void()> done) {
	const auto &ayu = AyuSettings::getInstance();
	struct Pending {
		AyuSettings::SentLocalGift sent;
		MsgId msgId = 0;
		std::optional<MTPStarGift> gift;
		bool giftRequested = false;
		bool finished = false;
		int attempts = 0;
	};
	struct State {
		not_null<PeerData*> peer;
		std::vector<Pending> pending;
		int remaining = 0;
		std::unique_ptr<base::Timer> timer;
	};
	auto pendings = std::vector<Pending>();
	for (const auto &sent : ayu.sentLocalGifts()) {
		if (sent.toId != to->id.value) {
			continue;
		}
		if (LocalTransferInjected.contains(sent.record.id)) {
			continue;
		}
		const auto msgId = MsgId(ayu.localTransferMsgId(sent.record.id));
		if (msgId.bare <= 0) {
			LOG((u"AyuGiftsMsg: skip transfer %1, no saved msg id"_q
				.arg(sent.record.id)));
			continue;
		}
		pendings.push_back({ sent, msgId, std::nullopt, false, false, 0 });
	}
	if (pendings.empty()) {
		return false;
	}
	const auto remaining = int(pendings.size());
	const auto state = std::make_shared<State>(State{
		to,
		std::move(pendings),
		remaining,
		nullptr,
	});
	const auto markFinished = [=](not_null<Pending*> pending) {
		pending->finished = true;
		if (--state->remaining == 0) {
			if (state->timer) {
				state->timer->cancel();
				state->timer = nullptr;
			}
			if (done) {
				done();
			}
		}
	};
	const auto findPending = [=](quint64 id) -> Pending* {
		for (auto &pending : state->pending) {
			if (pending.sent.record.id == id) {
				return &pending;
			}
		}
		return nullptr;
	};
	const auto trySwapNow = [=](not_null<Pending*> pending) {
		if (!pending->gift || pending->finished) {
			return;
		}
		const auto session = &state->peer->session();
		const auto history = session->data().history(state->peer);
		if (const auto item = session->data().message(
				state->peer->id,
				pending->msgId)) {
			if (item->isService()) {
				LOG((u"AyuGiftsMsg: already swapped dot %1 for transfer %2"_q
					.arg(pending->msgId.bare)
					.arg(pending->sent.record.id)));
				LocalTransferInjected.insert(pending->sent.record.id);
				markFinished(pending);
				return;
			}
			if (!item->out() || item->history() != history) {
				LOG((u"AyuGiftsMsg: dot %1 replaced, skip swap %2"_q
					.arg(pending->msgId.bare)
					.arg(pending->sent.record.id)));
				LocalTransferInjected.insert(pending->sent.record.id);
				markFinished(pending);
				return;
			}
			const auto date = item->date();
			history->destroyMessage(item);
			InsertTransferServiceMessage(
				session,
				history,
				state->peer,
				*pending->gift,
				pending->msgId,
				date,
				MessageFlags());
			LOG((u"AyuGiftsMsg: re-swapped dot %1 for transfer %2"_q
				.arg(pending->msgId.bare)
				.arg(pending->sent.record.id)));
			LocalTransferInjected.insert(pending->sent.record.id);
			markFinished(pending);
			return;
		}
		LOG((u"AyuGiftsMsg: dot %1 not loaded for transfer %2"_q
			.arg(pending->msgId.bare)
			.arg(pending->sent.record.id)));
	};
	const auto tick = [=] {
		for (auto &pending : state->pending) {
			if (pending.finished || !pending.gift) {
				continue;
			}
			if (++pending.attempts > kTransferRestoreAttemptBudget) {
				LOG((u"AyuGiftsMsg: give up restore for transfer %1"_q
					.arg(pending.sent.record.id)));
				LocalTransferInjected.insert(pending.sent.record.id);
				markFinished(&pending);
				continue;
			}
			trySwapNow(&pending);
		}
	};
	const auto requestGift = [=](quint64 id) {
		const auto session = &state->peer->session();
		const auto pending = findPending(id);
		if (!pending) {
			return;
		}
		pending->giftRequested = true;
		session->api().request(
			MTPpayments_GetUniqueStarGift(MTP_string(pending->sent.record.slug))
		).done([=](const MTPpayments_UniqueStarGift &result) {
			session->data().processUsers(result.data().vusers());
			if (const auto parsed = Api::FromTL(session, result.data().vgift())) {
				if (parsed->document) {
					parsed->document->createMediaView()->checkStickerLarge();
				}
			}
			if (const auto found = findPending(id)) {
				found->gift = ApplyLocalGiftRecordToMTP(
					&state->peer->session(),
					state->peer,
					found->sent.record,
					result.data().vgift());
				tick();
			}
		}).fail([=](const MTP::Error &) {
			if (const auto found = findPending(id)) {
				LOG((u"AyuGiftsMsg: gift fetch failed for %1"_q
					.arg(found->sent.record.id)));
				LocalTransferInjected.insert(found->sent.record.id);
				markFinished(found);
			}
		}).send();
	};
	state->timer = std::make_unique<base::Timer>([=] {
		tick();
	});
	state->timer->callEach(kTransferRestoreAttemptInterval);
	for (const auto &pending : state->pending) {
		requestGift(pending.sent.record.id);
	}
	return true;
}
void ShowTransferLocalGift(
		not_null<Window::SessionController*> controller,
		const AyuSettings::LocalGiftRecord &record) {
	auto saved = LocalGiftFromRecord(
		controller->session().user(),
		record,
		nullptr);
	if (!saved || !saved->info.unique) {
		return;
	}
	auto copy = std::make_shared<Data::UniqueGift>(*saved->info.unique);
	copy->starsForTransfer = 25;
	::ShowTransferGiftBox(
		controller,
		copy,
		Data::SavedStarGiftId::User(MsgId(1)));
}

bool ShowLocalGiftBySlug(
		not_null<Window::SessionController*> controller,
		const QString &slug) {
	if (const auto record = TryLocalGiftBySlug(slug)) {
		ShowLocalGiftBox(controller, *record);
		return true;
	}
	for (const auto &sent : AyuSettings::getInstance().sentLocalGifts()) {
		if (sent.record.slug.isEmpty()
			|| sent.record.slug.toLower() != slug.toLower()) {
			continue;
		}
		const auto peer = controller->session().data().peer(
			PeerId(sent.toId));
		if (!peer || peer->isSelf()) {
			return false;
		}
		if (auto gift = LocalGiftFromRecord(peer, sent.record, nullptr)) {
			::Settings::ShowSavedStarGiftBox(controller, peer, *gift);
			return true;
		}
		return false;
	}
	return false;
}

void ShowEditLocalGiftAttributes(
		not_null<Window::SessionController*> controller,
		quint64 id,
		Fn<void()> refresh) {
	const auto &ayu = AyuSettings::getInstance();
	const auto recordOpt = ayu.localGiftRecord(id);
	if (!recordOpt) {
		return;
	}
	const auto record = *recordOpt;
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(rpl::single(QString(u"Изменить подарок"_q)));
		box->setWidth(440);
		struct State {
			std::shared_ptr<Data::UniqueGiftAttributes> attributes;
			std::shared_ptr<Data::UniqueGift> base;
			std::shared_ptr<Data::UniqueGift> chosen;
			rpl::variable<Ui::UniqueGiftCover> *cover = nullptr;
		};
		const auto sel = box->lifetime().make_state<State>();
		const auto layout = box->verticalLayout();
		const auto peer = controller->session().user();
		if (auto saved = LocalGiftFromRecord(peer, record, nullptr)) {
			sel->base = saved->info.unique;
			const auto initial = Ui::UniqueGiftCover{
				.values = *sel->base,
				.force = true,
			};
			sel->cover = box->lifetime().make_state<
				rpl::variable<Ui::UniqueGiftCover>>(initial);
			Ui::AddUniqueGiftCover(
				layout,
				sel->cover->value(),
				{ .attributesInfo = true });
		}
		const auto addField = [&](const QString &ph, const QString &text) {
			return layout->add(object_ptr<Ui::InputField>(
				layout,
				st::defaultInputField,
				rpl::single(ph),
				text));
		};
		const auto model = addField(u"Модель"_q, record.modelName);
		const auto symbol = addField(u"Символ (узор)"_q, record.patternName);
		const auto backdrop = addField(u"Фон (название)"_q, record.backdropName);
		const auto color = addField(
			u"Цвет фона (hex #RRGGBB)"_q,
			record.backdropCenter.isValid()
				? record.backdropCenter.name(QColor::HexRgb)
				: QString());
		const auto priceField = addField(
			u"Цена (₽)"_q,
			QString::number(record.price));
		const auto countField = addField(
			u"Всего (limitedCount)"_q,
			record.limitedCount > 0
				? QString::number(record.limitedCount)
				: QString());
		const auto leftField = addField(
			u"Осталось (limitedLeft)"_q,
			record.limitedCount > 0
				? QString::number(record.limitedLeft)
				: QString());
		const auto applyChosen = [=](Data::UniqueGift chosen) {
			sel->base = std::make_shared<Data::UniqueGift>(std::move(chosen));
			sel->chosen = sel->base;
			model->setText(sel->base->model.name);
			symbol->setText(sel->base->pattern.name);
			backdrop->setText(sel->base->backdrop.name);
			color->setText(sel->base->backdrop.centerColor.name(
				QColor::HexRgb));
			if (sel->cover) {
				*sel->cover = Ui::UniqueGiftCover{
					.values = *sel->base,
					.force = true,
				};
			}
		};
		const auto variantsWrap = layout->add(
			object_ptr<Ui::VerticalLayout>(layout));
		auto sectionText = std::make_shared<rpl::variable<QString>>(
			u"Варианты с рынка:"_q);
		variantsWrap->add(object_ptr<Ui::FlatLabel>(
			variantsWrap,
			sectionText->value(),
			st::defaultSubsectionTitle),
			st::defaultSubsectionTitlePadding);
		const auto makePicker = [&](
				const QString &text,
				Data::GiftAttributeIdType tab) {
			auto button = object_ptr<Ui::SettingsButton>(
				variantsWrap,
				rpl::single(text),
				st::defaultSettingsButton);
			const auto raw = button.data();
			raw->setDisabled(true);
			raw->setClickedCallback([=] {
				if (!sel->attributes || !sel->base) {
					return;
				}
				box->getDelegate()->show(Box(
					Ui::StarGiftPreviewBox,
					text,
					*sel->attributes,
					tab,
					sel->base,
					[=](Data::UniqueGift chosen) {
						applyChosen(std::move(chosen));
					}));
			});
			variantsWrap->add(std::move(button));
			return raw;
		};
		const auto pickModel = makePicker(
			u"Выбрать модель"_q,
			Data::GiftAttributeIdType::Model);
		const auto pickPattern = makePicker(
			u"Выбрать символ (узор)"_q,
			Data::GiftAttributeIdType::Pattern);
		const auto pickBackdrop = makePicker(
			u"Выбрать фон"_q,
			Data::GiftAttributeIdType::Backdrop);
		const auto noVariants = [&] {
			*sectionText = u"Варианты недоступны."_q;
			pickModel->setDisabled(true);
			pickPattern->setDisabled(true);
			pickBackdrop->setDisabled(true);
			variantsWrap->update();
		};
		const auto weak = base::make_weak(box);
		if (record.initialGiftId) {
			controller->session().api().request(
				MTPpayments_GetStarGiftUpgradeAttributes(
					MTP_long(record.initialGiftId))
			).done([=](const MTPpayments_StarGiftUpgradeAttributes &result) {
				if (!weak) {
					return;
				}
				auto models = std::vector<Data::UniqueGiftModel>();
				auto patterns = std::vector<Data::UniqueGiftPattern>();
				auto backdrops = std::vector<Data::UniqueGiftBackdrop>();
				for (const auto &attribute : result.data().vattributes().v) {
					attribute.match(
					[&](const MTPDstarGiftAttributeModel &d) {
						models.push_back(Api::FromTL(
							&controller->session(),
							d));
					}, [&](const MTPDstarGiftAttributePattern &d) {
						patterns.push_back(Api::FromTL(
							&controller->session(),
							d));
					}, [&](const MTPDstarGiftAttributeBackdrop &d) {
						backdrops.push_back(Api::FromTL(d));
					}, [](const auto &) {});
				}
				if (models.empty() || patterns.empty() || backdrops.empty()) {
					noVariants();
					return;
				}
				sel->attributes = std::make_shared<Data::UniqueGiftAttributes>(
					Data::UniqueGiftAttributes{
						std::move(models),
						std::move(backdrops),
						std::move(patterns),
					});
				*sectionText = u"Варианты с рынка:"_q;
				pickModel->setDisabled(false);
				pickPattern->setDisabled(false);
				pickBackdrop->setDisabled(false);
				variantsWrap->update();
			}).fail([=](const MTP::Error &) {
				if (weak) {
					noVariants();
				}
			}).send();
		} else {
			noVariants();
		}
		box->addButton(rpl::single(QString(u"Применить"_q)), [=] {
			auto updated = record;
			updated.modelName = model->getTextWithTags().text.trimmed();
			updated.patternName = symbol->getTextWithTags().text.trimmed();
			updated.backdropName = backdrop->getTextWithTags().text.trimmed();
			if (sel->chosen) {
				const auto model = sel->chosen->model.document;
				const auto pattern = sel->chosen->pattern.document;
				updated.modelDocumentId = model->id;
				updated.modelRarity = sel->chosen->model.rarityValue;
				updated.patternDocumentId = pattern->id;
				updated.patternRarity = sel->chosen->pattern.rarityValue;
				const auto captureRemote = [](
						quint64 &access,
						QByteArray &fileReference,
						int32 &dc,
						int64 &size,
						int32 &date,
						QString &mime,
						QString &alt,
						not_null<DocumentData*> doc) {
					if (const auto input = doc->mtpInput();
						input.type() == mtpc_inputDocument) {
						access = input.c_inputDocument().vaccess_hash().v;
						fileReference = doc->fileReference();
						dc = doc->getDC();
						size = doc->size;
						date = doc->date;
						mime = doc->mimeString();
						if (const auto sticker = doc->sticker()) {
							alt = sticker->alt;
						}
					}
				};
				captureRemote(
					updated.modelAccess,
					updated.modelFileReference,
					updated.modelDc,
					updated.modelSize,
					updated.modelDate,
					updated.modelMime,
					updated.modelAlt,
					model);
				captureRemote(
					updated.patternAccess,
					updated.patternFileReference,
					updated.patternDc,
					updated.patternSize,
					updated.patternDate,
					updated.patternMime,
					updated.patternAlt,
					pattern);
				updated.backdropId = sel->chosen->backdrop.id;
				updated.backdropRarity = sel->chosen->backdrop.rarityValue;
				updated.backdropCenter = sel->chosen->backdrop.centerColor;
				updated.backdropEdge = sel->chosen->backdrop.edgeColor;
				updated.backdropPattern = sel->chosen->backdrop.patternColor;
				updated.backdropText = sel->chosen->backdrop.textColor;
				updated.backdropName = sel->chosen->backdrop.name;
			} else {
				const auto c = QColor(
					color->getTextWithTags().text.trimmed());
				if (c.isValid()) {
					updated.backdropCenter = c;
					updated.backdropEdge = c.lighter(130);
					updated.backdropPattern = c.lighter(130);
					updated.backdropText = QColor(255, 255, 255);
				}
			}
			const auto priceText = priceField->getTextWithTags().text.trimmed();
			if (!priceText.isEmpty()) {
				updated.price = priceText.toLongLong();
			}
			const auto countText = countField->getTextWithTags().text.trimmed();
			if (!countText.isEmpty()) {
				updated.limitedCount = countText.toInt();
			}
			const auto leftText = leftField->getTextWithTags().text.trimmed();
			if (!leftText.isEmpty()) {
				updated.limitedLeft = leftText.toInt();
			}
			AyuSettings::getInstance().addLocalGift(updated);
			auto &settings = AyuSettings::getInstance();
			if (settings.localProfileBackgroundGiftId() == updated.id) {
				settings.setLocalProfileBackgroundGiftId(0);
				settings.setLocalProfileBackgroundGiftId(updated.id);
			}
			if (refresh) {
				refresh();
			}
			box->closeBox();
		});
		box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
	}));
}
void ApplyGiftVariantDocRemote(
		not_null<Main::Session*> session,
		quint64 id,
		quint64 access,
		const QByteArray &fileReference,
		int32 dc,
		int64 size,
		int32 date,
		const QString &mime,
		const QString &alt) {
	if (!id || !access) {
		return;
	}
	const auto doc = session->data().document(id);
	if (doc->sticker()) {
		return;
	}
	auto attributes = QVector<MTPDocumentAttribute>();
	attributes.push_back(MTP_documentAttributeSticker(
		MTP_flags(0),
		MTP_string(alt),
		MTP_inputStickerSetEmpty(),
		MTPMaskCoords()));
	session->data().document(
		id,
		access,
		fileReference,
		date,
		attributes,
		mime,
		InlineImageLocation(),
		ImageWithLocation(),
		ImageWithLocation(),
		false,
		dc,
		size);
}
bool PreloadLocalGiftDocuments(
		not_null<Main::Session*> session,
		std::vector<AyuSettings::LocalGiftRecord> records,
		Fn<void()> done) {
	const auto pending = std::make_shared<int>(0);
	auto started = false;
	for (const auto &record : records) {
		if (record.modelDocumentId
			&& record.modelDocumentId != record.documentId) {
			ApplyGiftVariantDocRemote(
				session,
				record.modelDocumentId,
				record.modelAccess,
				record.modelFileReference,
				record.modelDc,
				record.modelSize,
				record.modelDate,
				record.modelMime,
				record.modelAlt);
		}
		if (record.patternDocumentId
			&& record.patternDocumentId != record.documentId) {
			ApplyGiftVariantDocRemote(
				session,
				record.patternDocumentId,
				record.patternAccess,
				record.patternFileReference,
				record.patternDc,
				record.patternSize,
				record.patternDate,
				record.patternMime,
				record.patternAlt);
		}
		if (record.slug.isEmpty()) {
			continue;
		}
		auto doc = record.documentId
			? session->data().document(record.documentId).get()
			: nullptr;
		if (doc && doc->sticker()) {
			continue;
		}
		++*pending;
		started = true;
		session->api().request(
			MTPpayments_GetUniqueStarGift(MTP_string(record.slug))
		).done([=](const MTPpayments_UniqueStarGift &result) {
			session->data().processUsers(result.data().vusers());
			Api::FromTL(session, result.data().vgift());
			if (--*pending == 0 && done) {
				done();
			}
		}).fail([=](const MTP::Error &) {
			if (--*pending == 0 && done) {
				done();
			}
		}).send();
	}
	return started;
}
std::optional<AyuSettings::LocalGiftRecord> TryLocalGiftBySlug(
		const QString &slug) {
	const auto &ayu = AyuSettings::getInstance();
	if (!ayu.localProfileEditor() || slug.isEmpty()) {
		return std::nullopt;
	}
	const auto lower = slug.toLower();
	for (const auto &record : ayu.localGiftRecords()) {
		if (record.slug.isEmpty() || record.slug.toLower() != lower) {
			continue;
		}
		return record;
	}
	return std::nullopt;
}

std::shared_ptr<Data::EmojiStatusCollectible> LocalProfileCollectible() {
	const auto &ayu = AyuSettings::getInstance();
	if (!ayu.localProfileEditor()) {
		return nullptr;
	}
	const auto id = ayu.localProfileBackgroundGiftId();
	if (!id) {
		return nullptr;
	}
	const auto record = ayu.localGiftRecord(id);
	if (!record || !record->backdropCenter.isValid()) {
		return nullptr;
	}
	auto collectible = std::make_shared<Data::EmojiStatusCollectible>(
		Data::EmojiStatusCollectible{
			.id = record->id,
			.documentId = DocumentId(record->modelDocumentId),
			.title = record->title,
			.slug = record->slug,
			.patternDocumentId = DocumentId(record->patternDocumentId),
			.centerColor = record->backdropCenter,
			.edgeColor = record->backdropEdge,
			.patternColor = record->backdropPattern.isValid()
				? record->backdropPattern
				: record->backdropEdge,
			.textColor = record->backdropText,
		});
	return collectible;
}

bool ShowLocalCollectible(
		not_null<Window::SessionController*> controller,
		const QString &entity) {
	const auto &ayu = AyuSettings::getInstance();
	if (!ayu.localProfileEditor()) {
		return false;
	}

	auto price = int64(0);
	auto date = TimeId(0);
	auto found = false;

	const auto stripped = entity.trimmed();
	auto strippedForMatch = stripped;
	if (strippedForMatch.startsWith('@')) {
		strippedForMatch = strippedForMatch.mid(1);
	}
	const auto strippedLower = strippedForMatch.toLower();

	if (stripped.startsWith('+')) {
		const auto localPhone = ayu.localProfilePhone();
		if (!localPhone.isEmpty()) {
			auto normalizedLocal = localPhone;
			normalizedLocal.remove(' ');
			normalizedLocal.remove('-');
			normalizedLocal.remove('(');
			normalizedLocal.remove(')');
			auto normalizedEntity = stripped;
			normalizedEntity.remove(' ');
			normalizedEntity.remove('-');
			normalizedEntity.remove('(');
			normalizedEntity.remove(')');
			if (normalizedLocal == normalizedEntity
				|| normalizedLocal == ('+' + normalizedEntity)) {
				price = ayu.localPhonePrice().toLongLong();
				date = ayu.localPhoneDate().toLongLong();
				found = true;
			}
		}
	} else {
		auto primary = ayu.localProfileUsername().toLower();
		if (primary.startsWith('@')) {
			primary = primary.mid(1);
		}
		if (!primary.isEmpty() && strippedLower == primary) {
			price = ayu.localUsernamePrice().toLongLong();
			date = ayu.localUsernameDate().toLongLong();
			found = true;
		} else {
			for (const auto &raw : ayu.localAdditionalUsernames()
				.split(',', Qt::SkipEmptyParts)) {
				const auto trimmed = raw.trimmed();
				const auto eq = trimmed.indexOf('=');
				if (eq < 0) continue;
				auto name = trimmed.mid(0, eq).trimmed().toLower();
				if (name.startsWith('@')) {
					name = name.mid(1);
				}
				if (name != strippedLower) continue;

				const auto rest = trimmed.mid(eq + 1).trimmed();
				const auto restParts = rest.split('=', Qt::SkipEmptyParts);
				if (!restParts.isEmpty()) {
					price = restParts[0].trimmed().toLongLong();
				}
				if (restParts.size() > 1) {
					date = restParts[1].trimmed().toLongLong();
				}
				found = true;
				break;
			}
		}
	}

	if (!found) {
		return false;
	}

	const auto session = &controller->session();
	const auto owner = session->user();
	auto info = Ui::CollectibleInfo{
		.entity = stripped,
		.copyText = session->createInternalLinkFull(stripped),
		.ownerUserpic = Ui::MakeUserpicThumbnail(owner, true),
		.ownerName = owner->name(),
		.cryptoAmount = uint64(price) * 1'000'000'000,
		.amount = TonsToUsdCents(price),
		.cryptoCurrency = u"TON"_q,
		.currency = u"USD"_q,
		.url = QString(),
		.date = date,
	};
	controller->show(Box(Ui::CollectibleInfoBox, std::move(info)));
	return true;
}

void ShowLocalGiftInfo(
		not_null<Window::SessionController*> controller,
		quint64 id) {
	const auto &ayu = AyuSettings::getInstance();
	if (!ayu.isLocalGiftId(id)) {
		return;
	}
	const auto recordOpt = ayu.localGiftRecord(id);
	if (!recordOpt) {
		return;
	}
	const auto peer = controller->session().user();
	if (auto saved = LocalGiftFromRecord(peer, *recordOpt, nullptr)) {
		::Settings::ShowSavedStarGiftBox(controller, peer, *saved);
	}
}

void ShowEditLocalGiftPrice(
		not_null<Window::SessionController*> controller,
		quint64 id) {
	const auto &ayu = AyuSettings::getInstance();
	if (!ayu.isLocalGiftId(id)) {
		return;
	}

	const auto oldPrice = ayu.localGiftPrice(id);

	controller->show(Box([id, oldPrice](not_null<Ui::GenericBox*> box) {
		box->setTitle(rpl::single(QString(u"Изменить цену подарка"_q)));

		const auto layout = box->verticalLayout();

		layout->add(object_ptr<Ui::FlatLabel>(
			layout,
			rpl::single(u"Подарок #%1"_q.arg(id)),
			st::defaultSubsectionTitle),
			st::defaultSubsectionTitlePadding);

		const auto currentText = u"Текущая цена: %1 ₽"_q.arg(oldPrice);
		layout->add(object_ptr<Ui::FlatLabel>(
			layout,
			rpl::single(currentText),
			st::defaultSubsectionTitle),
			st::defaultSubsectionTitlePadding);

		auto priceField = object_ptr<Ui::InputField>(
			layout,
			st::defaultInputField,
			rpl::single(QString(u"Новая цена (₽)"_q)),
			QString::number(oldPrice));
		const auto rawField = priceField.data();
		layout->add(
			std::move(priceField),
			st::defaultSubsectionTitlePadding);

		box->addButton(rpl::single(QString(u"Сохранить"_q)), [=] {
			const auto newPrice = rawField->getTextWithTags().text.toLongLong();
			if (newPrice < 0) return;

			AyuSettings::getInstance().updateLocalGift(
				id,
				newPrice,
				AyuSettings::getInstance().localGiftDate(id),
				AyuSettings::getInstance().localGiftTitle(id));

			box->closeBox();
		});
		box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
	}));
}
