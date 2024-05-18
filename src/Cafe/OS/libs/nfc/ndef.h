#pragma once

#include <span>
#include <vector>
#include <optional>

#include "stream.h"

namespace ndef
{

	class Record
	{
	public:
		enum HeaderFlag
		{
			NDEF_IL         = 0x08,
			NDEF_SR         = 0x10,
			NDEF_CF         = 0x20,
			NDEF_ME         = 0x40,
			NDEF_MB         = 0x80,
			NDEF_TNF_MASK   = 0x07,
		};

		enum TypeNameFormat
		{
			NDEF_TNF_EMPTY      = 0,
			NDEF_TNF_WKT        = 1,
			NDEF_TNF_MEDIA      = 2,
			NDEF_TNF_URI        = 3,
			NDEF_TNF_EXT        = 4,
			NDEF_TNF_UNKNOWN    = 5,
			NDEF_TNF_UNCHANGED  = 6,
			NDEF_TNF_RESERVED   = 7,
		};

	public:
		Record();
		virtual ~Record();

		static std::optional<Record> FromStream(Stream& stream);
		std::vector<std::byte> ToBytes(uint8 flags = 0) const;

		TypeNameFormat GetTNF() const;
		const std::vector<std::byte>& GetID() const;
		const std::vector<std::byte>& GetType() const;
		const std::vector<std::byte>& GetPayload() const;

		void SetTNF(TypeNameFormat tnf);
		void SetID(const std::span<const std::byte>& id);
		void SetType(const std::span<const std::byte>& type);
		void SetPayload(const std::span<const std::byte>& payload);

		bool IsLast() const;
		bool IsShort() const;

	private:
		uint8 mFlags;
		TypeNameFormat mTNF;
		std::vector<std::byte> mID;
		std::vector<std::byte> mType;
		std::vector<std::byte> mPayload;
	};

	class Message
	{
	public:
		Message();
		virtual ~Message();

		static std::optional<Message> FromBytes(const std::span<const std::byte>& data);
		std::vector<std::byte> ToBytes() const;

		Record& operator[](int i) { return mRecords[i]; }
		const Record& operator[](int i) const { return mRecords[i]; }

		void append(const Record& r);

		auto begin() { return mRecords.begin(); }
		auto end() { return mRecords.end(); }
		auto begin() const { return mRecords.begin(); }
		auto end() const { return mRecords.end(); }

	private:
		std::vector<Record> mRecords;
	};

} // namespace ndef
