// This is the source code of AyuGram for Desktop.
#pragma once

#include "ayu/ayu_settings.h"
#include "base/basic_types.h"

#include <memory>

class QString;
class PeerData;
class DocumentData;

namespace Data {
struct EmojiStatusCollectible;
struct SavedStarGift;
struct StarGift;
} // namespace Data

namespace Window {
class SessionController;
} // namespace Window

[[nodiscard]] AyuSettings::LocalGiftRecord MakeLocalGiftRecord(
	const Data::StarGift &info,
	int64 price);

[[nodiscard]] std::optional<Data::SavedStarGift> LocalGiftFromRecord(
	not_null<PeerData*> peer,
	const AyuSettings::LocalGiftRecord &record,
	DocumentData *fallbackDoc);

void SetLocalChatBackground(
	not_null<Window::SessionController*> controller,
	const AyuSettings::LocalGiftRecord &record);

void ShowLocalGiftBox(
	not_null<Window::SessionController*> controller,
	const AyuSettings::LocalGiftRecord &record);

void ShowTransferLocalGift(
	not_null<Window::SessionController*> controller,
	const AyuSettings::LocalGiftRecord &record);

void AddLocalTransferMessage(
	not_null<Main::Session*> session,
	not_null<PeerData*> to,
	const AyuSettings::LocalGiftRecord &record,
	Fn<void()> done = nullptr);

[[nodiscard]] bool PreloadLocalGiftDocuments(
	not_null<Main::Session*> session,
	std::vector<AyuSettings::LocalGiftRecord> records,
	Fn<void()> done = nullptr);

[[nodiscard]] bool EnsureLocalTransferMessages(
	not_null<PeerData*> to,
	Fn<void()> done = nullptr);

[[nodiscard]] std::optional<AyuSettings::LocalGiftRecord>
	TryLocalGiftBySlug(const QString &slug);

[[nodiscard]] bool ShowLocalGiftBySlug(
	not_null<Window::SessionController*> controller,
	const QString &slug);

[[nodiscard]] std::shared_ptr<Data::EmojiStatusCollectible>
	LocalProfileCollectible();

[[nodiscard]] bool ShowLocalCollectible(
	not_null<Window::SessionController*> controller,
	const QString &entity);

void ShowLocalGiftInfo(
	not_null<Window::SessionController*> controller,
	quint64 id);

void ShowEditLocalGiftPrice(
	not_null<Window::SessionController*> controller,
	quint64 id);

void ShowEditLocalGiftAttributes(
	not_null<Window::SessionController*> controller,
	quint64 id,
	Fn<void()> refresh = nullptr);
