// Copyright 2020 The Beam Team
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

#include "push_tx_builder.h"

#include "core/shielded.h"

namespace beam::wallet::lelantus
{
    PushTxBuilder::PushTxBuilder(BaseTransaction& tx)
        : BaseTxBuilder(tx, kDefaultSubTxID)
    {
        m_Value = m_Tx.GetMandatoryParameter<Amount>(TxParameterID::Amount);
        m_Tx.GetParameter(TxParameterID::AssetID, m_AssetID);

        ECC::Hash::Value hv;
        if (m_Tx.GetParameter(TxParameterID::KernelID, hv, m_SubTxID))
            m_Signing = Stage::Done;
    }

    void PushTxBuilder::SignSendShielded()
    {
        if (Stage::None != m_Signing)
            return;

        struct MyHandler
            :public KeyKeeperHandler
        {
            using KeyKeeperHandler::KeyKeeperHandler;

            IPrivateKeyKeeper2::Method::SignSendShielded m_Method;

            virtual ~MyHandler() {} // auto

            virtual void OnSuccess(BaseTxBuilder& b_) override
            {
                PushTxBuilder& b = Cast::Up<PushTxBuilder>(b_);

                ECC::Scalar::Native kOffs(b.m_pTransaction->m_Offset);
                kOffs += m_Method.m_kOffset;
                b.m_pTransaction->m_Offset = kOffs;

                b.m_Tx.SetParameter(TxParameterID::Kernel, m_Method.m_pKernel, b.m_SubTxID);
                b.m_Tx.SetParameter(TxParameterID::KernelID, m_Method.m_pKernel->m_Internal.m_ID, b.m_SubTxID);
                b.m_Tx.SetParameter(TxParameterID::Offset, b.m_pTransaction->m_Offset, b.m_SubTxID);
                b.m_Tx.SetParameter(TxParameterID::ShieldedSerialPub, m_Method.m_Voucher.m_Ticket.m_SerialPub);

                b.m_pTransaction->m_vKernels.push_back(std::move(m_Method.m_pKernel));

                if (b.VerifyTx())
                    OnAllDone(b);
                else
                    OnFailed(b, IPrivateKeyKeeper2::Status::Unspecified);
            }
        };

        KeyKeeperHandler::Ptr pHandler = std::make_shared<MyHandler>(*this, m_Signing);
        MyHandler& x = Cast::Up<MyHandler>(*pHandler);
        IPrivateKeyKeeper2::Method::SignSendShielded& m = x.m_Method;

        SetCommon(m);

        WalletID widMy = m_Tx.GetMandatoryParameter<WalletID>(TxParameterID::MyID);
        WalletID widPeer;
        bool bHasWidPeer = m_Tx.GetParameter(TxParameterID::PeerID, widPeer);

        if (!m_Tx.GetParameter(TxParameterID::PeerWalletIdentity, m.m_Peer))
        {
            auto wa = m_Tx.GetWalletDB()->getAddress(bHasWidPeer ? widPeer : widMy);
            if (!wa)
                throw TransactionFailedException(true, TxFailureReason::NoVouchers);

            m.m_Peer = wa->m_Identity;
            m.m_MyIDKey = wa->m_OwnID;
        }

        ShieldedVoucherList vouchers;
        if (!m_Tx.GetParameter(TxParameterID::UnusedShieldedVoucherList, vouchers))
        {
            if (!m_Tx.GetParameter(TxParameterID::ShieldedVoucherList, vouchers))
            {
                if (m.m_MyIDKey)
                {
                    // We're sending to ourselves. Create our voucher
                    IPrivateKeyKeeper2::Method::CreateVoucherShielded m2;
                    m2.m_MyIDKey = m.m_MyIDKey;
                    ECC::GenRandom(m2.m_Nonce);

                    if (IPrivateKeyKeeper2::Status::Success != m_Tx.get_KeyKeeperStrict()->InvokeSync(m2))
                        throw TransactionFailedException(true, TxFailureReason::KeyKeeperError);

                    vouchers.push_back(std::move(m2.m_Voucher));
                }
                else
                {
                    if (!bHasWidPeer)
                        throw TransactionFailedException(true, TxFailureReason::NoVouchers);

                    boost::optional<ShieldedTxo::Voucher> res;
                    m_Tx.GetGateway().get_UniqueVoucher(widPeer, m_Tx.GetTxID(), res);

                    if (!res)
                        return;

                    vouchers.push_back(std::move(*res));
                }
            }

            m_Tx.SetParameter(TxParameterID::UnusedShieldedVoucherList, vouchers);
        }

        m.m_Voucher = vouchers.back();
        vouchers.pop_back();
        m_Tx.SetParameter(TxParameterID::UnusedShieldedVoucherList, vouchers);

        ZeroObject(m.m_User);

        if (!m.m_MyIDKey)
        {
            auto wa = m_Tx.GetWalletDB()->getAddress(widMy);
            if (wa)
                m.m_User.m_Sender = wa->m_Identity;
        }

        // TODO: add ShieldedMessage if needed
        // m.m_User.m_Message = m_Tx.GetMandatoryParameter<WalletID>(TxParameterID::ShieldedMessage);

        ShieldedTxo::Viewer viewer;
        viewer.FromOwner(*m_Tx.GetWalletDB()->get_OwnerKdf(), 0);

        ShieldedTxo::DataParams pars;
        if (pars.m_Ticket.Recover(m.m_Voucher.m_Ticket, viewer))
        {
            // sending to yourself
            pars.m_Output.m_User = m.m_User;
            pars.m_Output.m_Value = m_Value;
            pars.m_Output.m_AssetID = m_AssetID;


            // save shielded Coin
            ShieldedCoin shieldedCoin;
            shieldedCoin.m_createTxId = m_Tx.GetTxID();

            shieldedCoin.m_CoinID.m_Key.m_nIdx = 0;
            pars.ToID(shieldedCoin.m_CoinID);

            m_Tx.GetWalletDB()->saveShieldedCoin(shieldedCoin);
        }

        m_Tx.get_KeyKeeperStrict()->InvokeAsync(m, pHandler);

    }

} // namespace beam::wallet::lelantus