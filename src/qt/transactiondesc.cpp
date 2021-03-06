// SPDX-FileCopyrightText: © 2020 Alias Developers
// SPDX-FileCopyrightText: © 2016 SpectreCoin Developers
// SPDX-FileCopyrightText: © 2011 Bitcoin Developers
//
// SPDX-License-Identifier: MIT

#include "transactiondesc.h"

#include "guiutil.h"
#include "bitcoinunits.h"

#include "main.h"
#include "wallet.h"
#include "txdb.h"
#include "interface.h"
#include "base58.h"

void toHTML(CWallet *wallet, CWalletTx &wtx, QString& strHTML, const bool& debit, const CTxDestination& destination, const std::vector<CTxDestination>& destSubs, const int64_t& amount, const Currency& currency, const std::string& narration, bool& narrationHandled);
void toHTML(CWallet *wallet, CWalletTx &wtx, QString& strHTML, const Currency& sCurrency, const CTxDestination& destination, const std::vector<CTxDestination>& destSubs, const int64_t& amount, const Currency& currency, const std::string& narration, bool& narrationHandled);
void toHTML(CWallet *wallet, QString& strHTML, const CTxDestination& destination, const std::vector<CTxDestination>& destSubs, const std::string& narration, bool& narrationHandled);
void toHTML(CWallet *wallet, QString& strHTML, const std::vector<CTxDestination>& destSubs);

static QString explorer;

bool addressLink(CWallet *wallet, const CTxDestination& destination, QString& strHTML)
{
    CBitcoinAddress address(destination);
    if (!address.IsValid())
        return false;

    strHTML += "<a href='javascript:void(0);' onclick='clientBridge.urlClicked(\""+ explorer+"address.dws?"+ GUIUtil::HtmlEscape(CBitcoinAddress(address).ToString()) + "\");'>" +
            ((wallet->mapAddressBook.count(destination) && !wallet->mapAddressBook[destination].empty()) ?
                 GUIUtil::HtmlEscape(wallet->mapAddressBook[destination]) : GUIUtil::HtmlEscape(address.ToString())) +
            "</a>";
    return true;
}

QString TransactionDesc::FormatTxStatus(const CWalletTx& wtx)
{
    AssertLockHeld(cs_main);
    if (!wtx.IsFinal())
    {
        if (wtx.nLockTime < LOCKTIME_THRESHOLD)
            return tr("Open for %n block(s)", "", nBestHeight - wtx.nLockTime);
        else
            return tr("Open until %1").arg(GUIUtil::dateTimeStr(wtx.nLockTime));
    } else
    {
        int nDepth = wtx.GetDepthInMainChain();
        if (nDepth < 0)
            return tr("conflicted");
        else if (GetAdjustedTime() - wtx.nTimeReceived > 2 * 60 && wtx.GetRequestCount() == 0)
            return tr("%1/offline").arg(nDepth);
        else if (nDepth < 10)
            return tr("%1/unconfirmed").arg(nDepth);
        else
            return tr("%1 confirmations").arg(nDepth);
    };
}

QString TransactionDesc::toHTML(CWallet *wallet, CWalletTx &wtx)
{
    if (explorer.isEmpty())
        explorer = fTestNet ? "https://chainz.cryptoid.info/alias-test/" : "https://chainz.cryptoid.info/alias/";

    QString strHTML;

    LOCK2(cs_main, wallet->cs_wallet);
    strHTML.reserve(4000);
    strHTML += "<html><font face='Montserrat, sans-serif'>";

    int64_t nTime = wtx.GetTxTime();
    int64_t nCredit = wtx.GetCredit();
    int64_t nDebit = wtx.GetDebit();
    int64_t nNet = nCredit - nDebit;

    QString txid(wtx.GetHash().ToString().c_str());
    strHTML += "<b>" + tr("Transaction ID") + ":</b> <a href='javascript:void(0);' onclick='clientBridge.urlClicked(\""+explorer+"tx.dws?" + txid +"\");'>" + txid + "</a><br>";
    if (wtx.hashBlock != 0)
    {
        QString blockHash = QString::fromStdString(wtx.hashBlock.GetHex());
        strHTML += "<b>" + tr("Block Hash") + ":</b> <a href='javascript:void(0);' onclick='clientBridge.urlClicked(\""+explorer+"block.dws?" + blockHash +"\");'>" + blockHash + "</a><br>";
    }

    strHTML += "<b>" + tr("Status") + ":</b> " + FormatTxStatus(wtx);
    int nRequests = wtx.GetRequestCount();
    if (nRequests != -1)
    {
        if (nRequests == 0)
            strHTML += tr(", has not been successfully broadcast yet");
        else if (nRequests > 0)
            strHTML += tr(", broadcast through %n node(s)", "", nRequests);
    };

    strHTML += "<br>";

    strHTML += "<b>" + tr("Date") + ":</b> " + (nTime ? GUIUtil::dateTimeStr(nTime) : "") + "<br>";

    bool narrationHandled = false;

    //
    // CoinBase/CoinStake
    //
    if (wtx.IsCoinBase() || wtx.IsCoinStake())
    {
        strHTML += "<b>" + tr("Source") + ":</b> " + tr("Generated") + "<br>";
        if (nCredit == 0)
        {
            int64_t nUnmatured = 0;
            BOOST_FOREACH(const CTxOut& txout, wtx.vout)
                nUnmatured += txout.IsAnonOutput() ? wallet->GetSpectreCredit(txout) : wallet->GetCredit(txout);
            strHTML += "<b>" + tr("Credit") + ":</b> ";
            if (wtx.IsInMainChain())
            {
                strHTML += BitcoinUnits::formatWithUnit(BitcoinUnits::ALIAS, nUnmatured);
                strHTML += wtx.IsAnonCoinStake() ? " (private)" : " (public)";
                strHTML += " (" + tr("matures in %n more block(s)", "", wtx.GetBlocksToMaturity()) + ")";
            }
            else
                strHTML += "(" + tr("not accepted") + ")";
            strHTML += "<br>";
        }
    }

    int64_t allFee;
    std::string strSentAccount;
    std::list<CTxDestinationDetail> listReceived;
    std::list<CTxDestinationDetail> listSent;

    wtx.GetDestinationDetails(listReceived, listSent, allFee, strSentAccount);

    if (listSent.size() > 0 && allFee > 0) {
        const auto & destination = listSent.front();
        strHTML += "<b>" + tr("Transaction fee") + ":</b> " + BitcoinUnits::formatWithUnitCurrency(BitcoinUnits::ALIAS, -allFee, destination.currency) + "<br>";
    }

    Currency netCurrency = PUBLIC;
    if (listSent.size() > 0)
        netCurrency = listSent.front().currency;
    else if (listReceived.size() > 0)
        netCurrency = listReceived.front().currency;

    if (wtx.GetBlocksToMaturity() == 0)
    {
        strHTML += "<b>" + tr("Net amount") + ":</b> " + BitcoinUnits::formatWithUnitCurrency(BitcoinUnits::ALIAS, nNet, netCurrency, true) + "<br>";
    }

    if (!(wtx.IsCoinBase() || wtx.IsCoinStake()) &&  listReceived.size() > 0 && listSent.size() > 0)
    {
        // Transfer within account
        const auto & destinationSent = listSent.front();
        for (const auto & destination: listReceived)
        {
             ::toHTML(wallet, wtx, strHTML, destinationSent.currency, destination.address, destination.vAddressElements, destination.amount, destination.currency, destination.narration, narrationHandled);
        }
    }
    else {
        for (const auto & destination : listReceived)
        {
             ::toHTML(wallet, wtx, strHTML, false, destination.address, destination.vAddressElements, destination.amount, destination.currency, destination.narration, narrationHandled);
        }
        for (const auto & destination : listSent)
        {
            if (wtx.IsCoinStake())
            {
                // only add contributions/donations
                std::string strAddress = CBitcoinAddress(destination.address).ToString();
                if (strAddress != Params().GetDevContributionAddress() && strAddress != Params().GetSupplyIncreaseAddress())
                    continue;
            }
            ::toHTML(wallet, wtx, strHTML, true, destination.address, destination.vAddressElements, destination.amount, destination.currency, destination.narration, narrationHandled);
        }
    }

    //
    // Message
    //
    if (wtx.mapValue.count("message") && !wtx.mapValue["message"].empty())
        strHTML += "<br><b>" + tr("Message") + ":</b><br>" + GUIUtil::HtmlEscape(wtx.mapValue["message"], true) + "<br>";
    if (wtx.mapValue.count("comment") && !wtx.mapValue["comment"].empty())
        strHTML += "<br><b>" + tr("Comment") + ":</b><br>" + GUIUtil::HtmlEscape(wtx.mapValue["comment"], true) + "<br>";
    if (wtx.mapValue.count("to") && !wtx.mapValue["to"].empty())
        strHTML += "<br><b>" + tr("Comment-To") + ":</b><br>" + GUIUtil::HtmlEscape(wtx.mapValue["to"], true) + "<br>";

    if (!narrationHandled) {
        char cbuf[256];
        for (uint32_t k = 0; k < wtx.vout.size(); ++k)
        {
            snprintf(cbuf, sizeof(cbuf), "n_%d", k);
            if (wtx.mapValue.count(cbuf) && !wtx.mapValue[cbuf].empty())
            strHTML += "<br><b>" + tr(cbuf) + ":</b> " + GUIUtil::HtmlEscape(wtx.mapValue[cbuf], true) + "<br>";
        }
    }

    if (wtx.IsCoinBase() || wtx.IsCoinStake())
        strHTML += "<br>" + tr("Generated coins must mature 450 blocks before they can be spent. When you generated this block, it was broadcast to the network to be added to the block chain. If it fails to get into the chain, its state will change to \"not accepted\" and it won't be spendable. This may occasionally happen if another node generates a block within a few seconds of yours.") + "<br>";

    //
    // Debug view
    //
    if (fDebug)
    {
        strHTML += "<hr>" + tr("Debug information") + "<br><br>";
        BOOST_FOREACH(const CTxIn& txin, wtx.vin)
            if(wallet->IsMine(txin))
                strHTML += "<b>" + tr("Debit") + ":</b> " + BitcoinUnits::formatWithUnit(BitcoinUnits::ALIAS, -wallet->GetDebit(txin)) + "<br>";
        BOOST_FOREACH(const CTxOut& txout, wtx.vout)
            if(wallet->IsMine(txout))
                strHTML += "<b>" + tr("Credit") + ":</b> " + BitcoinUnits::formatWithUnit(BitcoinUnits::ALIAS, wallet->GetCredit(txout)) + "<br>";

        strHTML += "<br><b>" + tr("Transaction") + ":</b><br>";
        strHTML += GUIUtil::HtmlEscape(wtx.ToString(), true);

        CTxDB txdb("r"); // To fetch source txouts

        strHTML += "<br><b>" + tr("Inputs") + ":</b>";
        strHTML += "<ul>";

        BOOST_FOREACH(const CTxIn& txin, wtx.vin)
        {
            COutPoint prevout = txin.prevout;

            if (wtx.nVersion == ANON_TXN_VERSION
                && txin.IsAnonInput())
            {
                std::vector<uint8_t> vchImage;
                txin.ExtractKeyImage(vchImage);
                int nRingSize = txin.ExtractRingSize();

                std::string sCoinValue;
                CKeyImageSpent ski;
                bool fInMemPool;
                if (GetKeyImage(&txdb, vchImage, ski, fInMemPool))
                {
                    sCoinValue = strprintf("%f", (double)ski.nValue / (double)COIN);
                } else
                {
                    sCoinValue = "spend not in chain!";
                };

                strHTML += "<li>Anon Coin - value: "+GUIUtil::HtmlEscape(sCoinValue)+", ring size: "+GUIUtil::HtmlEscape(itostr(nRingSize))+", keyimage: "+GUIUtil::HtmlEscape(HexStr(vchImage))+"</li>";
                continue;
            };

            CTransaction prev;
            if (txdb.ReadDiskTx(prevout.hash, prev))
            {
                if (prevout.n < prev.vout.size())
                {
                    strHTML += "<li>";
                    const CTxOut &vout = prev.vout[prevout.n];
                    CTxDestination address;
                    if (ExtractDestination(vout.scriptPubKey, address))
                        addressLink(wallet, address, strHTML);

                    strHTML = strHTML + " " + tr("Amount") + "=" + BitcoinUnits::formatWithUnit(BitcoinUnits::ALIAS, vout.nValue);
                    strHTML = strHTML + " IsMine=" + (wallet->IsMine(vout) ? tr("true") : tr("false")) + "</li>";
                }
            }
        }

        strHTML += "</ul>";
    }

    strHTML += "</font></html>";
    return strHTML;
}

void toHTML(CWallet *wallet, CWalletTx &wtx, QString& strHTML, const bool& debit, const CTxDestination& destination, const std::vector<CTxDestination>& destSubs, const int64_t& amount, const Currency& currency, const std::string& narration, bool& narrationHandled)
{
    strHTML += "<hr/><dl>";
    strHTML += "<dt><b>" + TransactionDesc::tr(debit ? !wtx.IsCoinStake() ? "Debit" : wtx.GetDepthAndHeightInMainChain().second % 6 == 0 ? "Contributed" : "Donated" :
                                               wtx.IsCoinBase() ? "Mined" : wtx.IsCoinStake() ? "Staked" : "Credit") + ":</b></dt><dd>";
    strHTML += BitcoinUnits::formatWithUnitCurrency(BitcoinUnits::ALIAS, debit ? -amount : amount, currency) + "</dd>";
    strHTML += "<dt><b>" + TransactionDesc::tr(debit ? "Sent to": "With") + ":</b></dt><dd>";

    toHTML(wallet, strHTML, destination, currency == PUBLIC ? destSubs : std::vector<CTxDestination>(), narration, narrationHandled);

    strHTML += "</dl>";
}

void toHTML(CWallet *wallet, CWalletTx &wtx, QString& strHTML, const Currency& sCurrency, const CTxDestination& destination, const std::vector<CTxDestination>& destSubs, const int64_t& amount, const Currency& currency, const std::string& narration, bool& narrationHandled)
{
    strHTML += "<hr/><dl>";
    if (sCurrency == currency) {
        strHTML += "<dt><b>" + TransactionDesc::tr(wtx.IsCoinBase() ? "Mined" : wtx.IsCoinStake() ? "Staked" : "Sent to self") + ":</b></dt><dd>";
        strHTML += BitcoinUnits::formatWithUnitCurrency(BitcoinUnits::ALIAS, amount, currency) + "</dd>";
    }
    else {
        if (sCurrency == PUBLIC && currency == PRIVATE)
            strHTML += "<dt><b>" + TransactionDesc::tr("Converted") + ":</b></dt><dd>"
                    + BitcoinUnits::formatWithUnit(BitcoinUnits::ALIAS, amount) + TransactionDesc::tr(" from public to private") + "</dd>";
        else
            strHTML += "<dt><b>" + TransactionDesc::tr("Converted") + ":</b></dt><dd>"
                    + BitcoinUnits::formatWithUnit(BitcoinUnits::ALIAS, amount) + TransactionDesc::tr(" from private to public") + "</dd>";
    }

    strHTML += "<dt><b>" + TransactionDesc::tr("Address") + ":</b></dt><dd>";

    toHTML(wallet, strHTML, destination, currency == PUBLIC ? destSubs : std::vector<CTxDestination>(), narration, narrationHandled);

    strHTML += "</dl>";
}

void toHTML(CWallet *wallet, QString& strHTML, const CTxDestination& destination, const std::vector<CTxDestination>& destSubs, const std::string& narration, bool& narrationHandled)
{
    if (destination.type() == typeid(CStealthAddress)) {
        CStealthAddress stealthAddress = boost::get<CStealthAddress>(destination);
        if (!stealthAddress.label.empty())
            strHTML += GUIUtil::HtmlEscape(stealthAddress.label) + "<br>";
        strHTML += GUIUtil::HtmlEscape(stealthAddress.Encoded()) + "<br>";
        toHTML(wallet, strHTML, destSubs);

        narrationHandled = true;
        if (!narration.empty())
            strHTML += "<dt><b>" + TransactionDesc::tr("Note") + ":</b></dt><dd>" + GUIUtil::HtmlEscape(narration) + "</dd>";
    }
    else {
        if (addressLink(wallet, destination, strHTML))
        {
            strHTML += "</dd>";
            narrationHandled = true;
            if (!narration.empty())
                strHTML += "<dt><b>" + TransactionDesc::tr("Note") + ":</b></dt><dd>" + GUIUtil::HtmlEscape(narration) + "</dd>";
        }
        else
            strHTML += TransactionDesc::tr("unknown") + "</dd>";
    }
}

void toHTML(CWallet *wallet, QString& strHTML, const std::vector<CTxDestination>& destSubs)
{
    for (const auto& destination : destSubs) {
        QString strAddress;
        if (addressLink(wallet, destination, strAddress))
        {
            strHTML += "<dt>-</dt>";
            strHTML += TransactionDesc::tr("<dd>");
            strHTML += strAddress;
            strHTML += "</dd>";
        }
    }

}
