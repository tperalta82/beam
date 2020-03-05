// Copyright 2019 The Beam Team
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.


#include "common.h"
#include "utility/logger.h"
#include "core/ecc_native.h"
#include "base58.h"
#include "utility/string_helpers.h"

#include <iomanip>
#include <boost/algorithm/string.hpp>

using namespace std;
using namespace ECC;
using namespace beam;

namespace std
{
    string to_string(const beam::wallet::WalletID& id)
    {
        static_assert(sizeof(id) == sizeof(id.m_Channel) + sizeof(id.m_Pk), "");

        char szBuf[sizeof(id) * 2 + 1];
        beam::to_hex(szBuf, &id, sizeof(id));

        const char* szPtr = szBuf;
        while (*szPtr == '0')
            szPtr++;

        if (!*szPtr)
            szPtr--; // leave at least 1 symbol

        return szPtr;
    }

    string to_string(const Merkle::Hash& hash)
    {
        char sz[Merkle::Hash::nTxtLen + 1];
        hash.Print(sz);
        return string(sz);
    }

    string to_string(const beam::wallet::PrintableAmount& amount)
    {
        stringstream ss;

        if (amount.m_showPoint)
        {
            size_t maxGrothsLength = std::lround(std::log10(Rules::Coin));
            ss << fixed << setprecision(maxGrothsLength) << double(amount.m_value) / Rules::Coin;
            string s = ss.str();
            boost::algorithm::trim_right_if(s, boost::is_any_of("0"));
            boost::algorithm::trim_right_if(s, boost::is_any_of(",."));
            return s;
        }
        else
        {
            if (amount.m_value >= Rules::Coin)
            {
                ss << Amount(amount.m_value / Rules::Coin) << " " << (amount.m_coinName.empty() ? "beams" : amount.m_coinName);
            }
            Amount c = amount.m_value % Rules::Coin;
            if (c > 0 || amount.m_value == 0)
            {
                ss << (amount.m_value >= Rules::Coin ? (" ") : "") << c << " " << (amount.m_grothName.empty() ? "groth" : amount.m_grothName);
            }
            return ss.str();
        }
    }

    string to_string(const beam::wallet::TxParameters& value)
    {
        beam::wallet::TxToken token(value);
        Serializer s;
        s & token;
        ByteBuffer buffer;
        s.swap_buf(buffer);
        return beam::wallet::EncodeToBase58(buffer);
    }

    string to_string(const beam::Version& v)
    {
        return v.to_string();
    }
}  // namespace std

namespace beam
{
    std::ostream& operator<<(std::ostream& os, const wallet::TxID& uuid)
    {
        stringstream ss;
        ss << "[" << to_hex(uuid.data(), uuid.size()) << "]";
        os << ss.str();
        return os;
    }

    std::ostream& operator<<(std::ostream& os, const wallet::PrintableAmount& amount)
    {
        os << std::to_string(amount);
        
        return os;
    }

    std::string Version::to_string() const
    {
        std::string maj(std::to_string(m_major));
        std::string min(std::to_string(m_minor));
        std::string rev(std::to_string(m_revision));
        std::string res;
        res.reserve(maj.size() + min.size() + rev.size());
        res.append(maj).push_back('.');
        res.append(min).push_back('.');
        res.append(rev);
        return res;
    }

    bool Version::from_string(const std::string& verString)
    {
        try
        {
            auto stringList = string_helpers::split(verString, '.');
            if (stringList.size() != 3) return false;

            std::vector<uint32_t> verList;

            for (const auto& str : stringList)
            {
                size_t strEnd = 0;
                uint32_t integer = std::stoul(str, &strEnd);
                if (strEnd != str.size())
                {
                    return false;
                }
                verList.push_back(integer);
            }

            m_major = verList[0];
            m_minor = verList[1];
            m_revision = verList[2];
        }
        catch(...)
        {
            return false;
        }
        return true;
    }

    bool Version::operator==(const Version& other) const
    {
        return m_major == other.m_major
            && m_minor == other.m_minor
            && m_revision == other.m_revision;
    }

    bool Version::operator<(const Version& other) const
    {
        return m_major < other.m_major
            || (m_major == other.m_major
                && (m_minor < other.m_minor
                    || (m_minor == other.m_minor && m_revision < other.m_revision)));
    }

    bool Version::operator!=(const Version& other) const
    {
        return !(*this == other);
    }
}  // namespace beam

namespace beam::wallet
{
    int WalletID::cmp(const WalletID& x) const
    {
        int n = m_Channel.cmp(x.m_Channel);
        if (n)
            return n;
        return m_Pk.cmp(x.m_Pk);
    }

    bool WalletID::FromBuf(const ByteBuffer& x)
    {
        if (x.size() > sizeof(*this))
            return false;

        typedef uintBig_t<sizeof(*this)> BigSelf;
        static_assert(sizeof(BigSelf) == sizeof(*this), "");

        *reinterpret_cast<BigSelf*>(this) = Blob(x);
        return true;
    }

    bool WalletID::FromHex(const std::string& s)
    {
        bool bValid = true;
        ByteBuffer bb = from_hex(s, &bValid);

        return bValid && FromBuf(bb);
    }

    bool WalletID::IsValid() const
    {
        Point::Native p;
        return m_Pk.ExportNnz(p);
    }

    ByteBuffer toByteBuffer(const ECC::Point::Native& value)
    {
        ECC::Point pt;
        if (value.Export(pt))
        {
            return toByteBuffer(pt);
        }
        return ByteBuffer();
    }

    ByteBuffer toByteBuffer(const ECC::Scalar::Native& value)
    {
        ECC::Scalar s;
        value.Export(s);
        return toByteBuffer(s);
    }

    Amount GetMinimumFee(size_t numberOfOutputs, size_t numberOfKenrnels /*= 1*/)
    {
        // Minimum Fee = (number of outputs) * 10 + (number of kernels) * 10
        return (numberOfOutputs + numberOfKenrnels) * 10;
    }

    ErrorType getWalletError(proto::NodeProcessingException::Type exceptionType)
    {
        switch (exceptionType)
        {
        case proto::NodeProcessingException::Type::Incompatible:
            return ErrorType::NodeProtocolIncompatible;
        case proto::NodeProcessingException::Type::TimeOutOfSync:
            return ErrorType::TimeOutOfSync;
        default:
            return ErrorType::NodeProtocolBase;
        }
    }

    ErrorType getWalletError(io::ErrorCode errorCode)
    {
        switch (errorCode)
        {
        case io::ErrorCode::EC_ETIMEDOUT:
            return ErrorType::ConnectionTimedOut;
        case io::ErrorCode::EC_ECONNREFUSED:
            return ErrorType::ConnectionRefused;
        case io::ErrorCode::EC_EHOSTUNREACH:
            return ErrorType::ConnectionHostUnreach;
        case io::ErrorCode::EC_EADDRINUSE:
            return ErrorType::ConnectionAddrInUse;
        case io::ErrorCode::EC_HOST_RESOLVED_ERROR:
            return ErrorType::HostResolvedError;
        default:
            return ErrorType::ConnectionBase;
        }
    }

    bool ConfirmationBase::IsValid(const PeerID& pid) const
    {
        Point::Native pk;
        if (!pid.ExportNnz(pk))
            return false;

        Hash::Value hv;
        get_Hash(hv);

        return m_Signature.IsValid(hv, pk);
    }

    void ConfirmationBase::Sign(const Scalar::Native& sk)
    {
        Hash::Value hv;
        get_Hash(hv);

        m_Signature.Sign(hv, sk);
    }

    void PaymentConfirmation::get_Hash(Hash::Value& hv) const
    {
        Hash::Processor hp;
        hp
            << "PaymentConfirmation"
            << m_KernelID
            << m_Sender
            << m_Value;

        if (m_AssetID)
        {
            hp
                << "asset"
                << m_AssetID;
        }

        hp >> hv;
    }

    void SwapOfferConfirmation::get_Hash(Hash::Value& hv) const
    {
        beam::Blob data(m_offerData);
        Hash::Processor()
            << "SwapOfferSignature"
            << data
            >> hv;
    }

    void SignatureHandler::get_Hash(Hash::Value& hv) const
    {
        beam::Blob data(m_data);
        Hash::Processor()
            << "Undersign"
            << data
            >> hv;
    }

    TxParameters::TxParameters(const boost::optional<TxID>& txID)
        : m_ID(txID)
    {

    }

    bool TxParameters::operator==(const TxParameters& other)
    {
        return m_ID == other.m_ID &&
            m_Parameters == other.m_Parameters;
    }

    bool TxParameters::operator!=(const TxParameters& other)
    {
        return !(*this == other);
    }

    const boost::optional<TxID>& TxParameters::GetTxID() const
    {
        return m_ID;
    }

    boost::optional<ByteBuffer> TxParameters::GetParameter(TxParameterID parameterID, SubTxID subTxID) const
    {
        auto subTxIt = m_Parameters.find(subTxID);
        if (subTxIt == m_Parameters.end())
        {
            return {};
        }
        auto it = subTxIt->second.find(parameterID);
        if (it == subTxIt->second.end())
        {
            return {};
        }
        return boost::optional<ByteBuffer>(it->second);
    }

    TxParameters& TxParameters::SetParameter(TxParameterID parameterID, const ByteBuffer& parameter, SubTxID subTxID)
    {
        m_Parameters[subTxID][parameterID] = parameter;
        return *this;
    }

    PackedTxParameters TxParameters::Pack() const
    {
        PackedTxParameters parameters;
        for (const auto& subTx : m_Parameters)
        {
            if (subTx.first > kDefaultSubTxID)
            {
                parameters.emplace_back(TxParameterID::SubTxIndex, toByteBuffer(subTx.first));
            }
            for (const auto& p : subTx.second)
            {
                parameters.emplace_back(p.first, p.second);
            }
        }
        return parameters;
    }

    TxToken::TxToken(const TxParameters& parameters)
        : m_Flags(TxToken::TokenFlag)
        , m_TxID(parameters.GetTxID())
        , m_Parameters(parameters.Pack())
    {

    }

    TxParameters TxToken::UnpackParameters() const
    {
        TxParameters result(m_TxID);

        SubTxID subTxID = kDefaultSubTxID;
        Deserializer d;
        for (const auto& p : m_Parameters)
        {
            if (p.first == TxParameterID::SubTxIndex)
            {
                // change subTxID
                d.reset(p.second.data(), p.second.size());
                d & subTxID;
                continue;
            }

            result.SetParameter(p.first, p.second, subTxID);
        }
        return result;
    }

    boost::optional<TxParameters> ParseParameters(const string& text)
    {
        bool isValid = true;
        ByteBuffer buffer = from_hex(text, &isValid);
        if (!isValid)
        {
            buffer = DecodeBase58(text);
            if (buffer.empty())
            {
                return {};
            }
        }

        if (buffer.size() < 2)
        {
            return {};
        }

        if (buffer.size() > 33 && buffer[0] & TxToken::TokenFlag) // token
        {
            try
            {
                TxToken token;
                // simply deserialize for now
                Deserializer d;
                d.reset(&buffer[0], buffer.size());
                d & token;

                return boost::make_optional<TxParameters>(token.UnpackParameters());
            }
            catch (...)
            {
                // failed to deserialize
            }
        }
        else // plain WalletID
        {
            WalletID walletID;
            if (walletID.FromBuf(buffer))
            {
                auto result = boost::make_optional<TxParameters>({});
                result->SetParameter(TxParameterID::PeerID, walletID);
                return result;
            }
        }
        return {};
    }

    bool LoadReceiverParams(const TxParameters& receiverParams, TxParameters& params)
    {
        bool res = false;
        const TxParameters& p = receiverParams;
        if (auto peerID = p.GetParameter<WalletID>(beam::wallet::TxParameterID::PeerID); peerID)
        {
            params.SetParameter(beam::wallet::TxParameterID::PeerID, *peerID);
            res = true;
        }
        if (auto peerID = p.GetParameter<PeerID>(beam::wallet::TxParameterID::PeerSecureWalletID); peerID)
        {
            params.SetParameter(beam::wallet::TxParameterID::PeerSecureWalletID, *peerID);
            res &= true;
        }
        return res;
    }

    bool IsValidTimeStamp(Timestamp currentBlockTime_s)
    {
        Timestamp currentTime_s = getTimestamp();
        const Timestamp tolerance_s = 60 * 10; // 10 minutes tolerance.

        if (currentTime_s > currentBlockTime_s + tolerance_s)
        {
            LOG_INFO() << "It seems that last known blockchain tip is not up to date";
            return false;
        }
        return true;
    }

    bool TxDescription::canResume() const
    {
        return m_status == TxStatus::Pending
            || m_status == TxStatus::InProgress
            || m_status == TxStatus::Registering;
    }

    bool TxDescription::canCancel() const
    {
        return m_status == TxStatus::InProgress
            || m_status == TxStatus::Pending;
    }

    bool TxDescription::canDelete() const
    {
        return m_status == TxStatus::Failed
            || m_status == TxStatus::Completed
            || m_status == TxStatus::Canceled;
    }

    std::string TxDescription::getStatusString() const
    {
        const auto& statusStr = getStatusStringApi();
        if (statusStr == "receiving" || statusStr == "sending")
        {
            return "in progress";
        }
        else if (statusStr == "completed")
        {
            return "sent to own address";
        }
        else if (statusStr == "self sending")
        {
            return "sending to own address";
        }
        return statusStr;
    }

    std::string TxDescription::getStatusStringApi() const
    {
        switch (m_status)
        {
        case TxStatus::Pending:
            return "pending";
        case TxStatus::InProgress:
        {
            if (m_selfTx)
            {
                return "self sending";
            }
            return m_sender == false ? "waiting for sender" : "waiting for receiver";
        }
        case TxStatus::Registering:
        {
            if (m_selfTx)
            {
                return "self sending";
            }
            return m_sender == false ? "receiving" : "sending";
        }
        case TxStatus::Completed:
        {
            if (m_selfTx)
            {
                return "completed";
            }
            return m_sender == false ? "received" : "sent";
        }
        case TxStatus::Canceled:
            return "cancelled";
        case TxStatus::Failed:
            if (TxFailureReason::TransactionExpired == m_failureReason)
            {
                return "expired";
            }
            return "failed";
        default:
            break;
        }

        assert(false && "Unknown TX status!");
        return "unknown";
    }

    uint64_t get_RandomID()
    {
        uintBigFor<uint64_t>::Type val;
        ECC::GenRandom(val);

        uint64_t ret;
        val.Export(ret);
        return ret;
    }
}  // namespace beam::wallet
