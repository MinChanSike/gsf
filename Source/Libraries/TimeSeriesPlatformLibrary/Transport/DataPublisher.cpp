//******************************************************************************************************
//  DataPublisher.cpp - Gbtc
//
//  Copyright � 2018, Grid Protection Alliance.  All Rights Reserved.
//
//  Licensed to the Grid Protection Alliance (GPA) under one or more contributor license agreements. See
//  the NOTICE file distributed with this work for additional information regarding copyright ownership.
//  The GPA licenses this file to you under the MIT License (MIT), the "License"; you may not use this
//  file except in compliance with the License. You may obtain a copy of the License at:
//
//      http://opensource.org/licenses/MIT
//
//  Unless agreed to in writing, the subject software distributed under the License is distributed on an
//  "AS-IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. Refer to the
//  License for the specific language governing permissions and limitations.
//
//  Code Modification History:
//  ----------------------------------------------------------------------------------------------------
//  10/25/2018 - J. Ritchie Carroll
//       Generated original version of source code.
//
//******************************************************************************************************

#include "DataPublisher.h"
#include "Constants.h"
#include "../Common/Convert.h"
#include "../Common/EndianConverter.h"
#include "MetadataSchema.h"
#include "ActiveMeasurementsSchema.h"

using namespace std;
using namespace boost;
using namespace boost::asio;
using namespace boost::asio::ip;
using namespace GSF;
using namespace GSF::Data;
using namespace GSF::FilterExpressions;
using namespace GSF::TimeSeries;
using namespace GSF::TimeSeries::Transport;

inline int32_t GetColumnIndex(const DataTablePtr& table, const string& columnName)
{
    const DataColumnPtr& column = table->Column(columnName);

    if (column == nullptr)
        throw PublisherException("Column name \"" + columnName + "\" was not found in table \"" + table->Name() + "\"");

    return column->Index();
}

// --- ClientConnectedInfo ---

struct ClientConnectionInfo
{
    const GSF::Guid SubscriberID;
    const string ConnectionID;

    ClientConnectionInfo(const GSF::Guid& clientID, const string& connectionID) :
        SubscriberID(clientID),
        ConnectionID(connectionID)
    {
    }
};

// --- ClientConnection ---

ClientConnection::ClientConnection(DataPublisherPtr parent, IOContext& commandChannelService, IOContext& dataChannelService) :
    m_parent(std::move(parent)),
    m_commandChannelService(commandChannelService),
    m_subscriberID(NewGuid()),
    m_operationalModes(OperationalModes::NoFlags),
    m_encoding(OperationalEncoding::UTF8),
    m_usePayloadCompression(false),
    m_useCompactMeasurementFormat(true),
    m_isSubscribed(false),
    m_stopped(true),
    m_commandChannelSocket(m_commandChannelService),
    m_readBuffer(Common::MaxPacketSize),
    m_udpPort(0),
    m_dataChannelSocket(dataChannelService),
    m_timeIndex(0),
    m_baseTimeOffsets{0L, 0L}
{
    // Setup ping timer
    m_pingTimer.SetInteval(5000);
    m_pingTimer.SetAutoReset(true);
    m_pingTimer.SetCallback(&ClientConnection::PingTimerElapsed);
    m_pingTimer.SetUserData(this);
}

ClientConnection::~ClientConnection() = default;

TcpSocket& ClientConnection::CommandChannelSocket()
{
    return m_commandChannelSocket;
}

const GSF::Guid& ClientConnection::GetSubscriberID() const
{
    return m_subscriberID;
}

void ClientConnection::SetSubscriberID(const GSF::Guid& id)
{
    m_subscriberID = id;
}

const std::string& ClientConnection::GetConnectionID() const
{
    return m_connectionID;
}

const GSF::IPAddress& ClientConnection::GetIPAddress() const
{
    return m_ipAddress;
}

const std::string& ClientConnection::GetHostName() const
{
    return m_hostName;
}

uint32_t ClientConnection::GetOperationalModes() const
{
    return m_operationalModes;
}

void ClientConnection::SetOperationalModes(uint32_t value)
{
    m_operationalModes = value;
    m_encoding = m_operationalModes & OperationalModes::EncodingMask;
}

uint32_t ClientConnection::GetEncoding() const
{
    return m_encoding;
}

bool ClientConnection::GetUsePayloadCompression() const
{
    return m_usePayloadCompression;
}

void ClientConnection::SetUsePayloadCompression(bool value)
{
    m_usePayloadCompression = value;
}

bool ClientConnection::GetUseCompactMeasurementFormat() const
{
    return m_useCompactMeasurementFormat;
}

void ClientConnection::SetUseCompactMeasurementFormat(bool value)
{
    m_useCompactMeasurementFormat = value;
}

bool ClientConnection::GetIsSubscribed() const
{
    return m_isSubscribed;
}

void ClientConnection::SetIsSubscribed(bool value)
{
    m_isSubscribed = value;
}

const string& ClientConnection::GetSubscriptionInfo() const
{
    return m_subscriptionInfo;
}

void ClientConnection::SetSubscriptionInfo(const string& value)
{
    if (value.empty())
    {
        m_subscriptionInfo = "";
        return;
    }

    const StringMap<string> settings = ParseKeyValuePairs(value);
    string source, version, buildDate;

    TryGetValue(settings, "source", source);
    TryGetValue(settings, "version", version);
    TryGetValue(settings, "buildDate", buildDate);

    if (source.empty())
        source = "unknown source";

    if (version.empty())
        version = "?.?.?.?";

    if (buildDate.empty())
        buildDate = "undefined date";

    m_subscriptionInfo = source + " version " + version + " built on " + buildDate;
}

const SignalIndexCachePtr& ClientConnection::GetSignalIndexCache() const
{
    return m_signalIndexCache;
}

void ClientConnection::SetSignalIndexCache(SignalIndexCachePtr signalIndexCache)
{
    m_signalIndexCache = std::move(signalIndexCache);
}

bool ClientConnection::CipherKeysDefined() const
{
    return !m_keys[0].empty();
}

vector<uint8_t> ClientConnection::Keys(int32_t cipherIndex)
{
    if (cipherIndex < 0 || cipherIndex > 1)
        throw out_of_range("Cipher index must be 0 or 1");

    return m_keys[cipherIndex];
}

vector<uint8_t> ClientConnection::IVs(int32_t cipherIndex)
{
    if (cipherIndex < 0 || cipherIndex > 1)
        throw out_of_range("Cipher index must be 0 or 1");

    return m_ivs[cipherIndex];
}

void ClientConnection::Start()
{
    // Attempt to lookup remote connection identification for logging purposes
    auto remoteEndPoint = m_commandChannelSocket.remote_endpoint();
    m_ipAddress = remoteEndPoint.address();

    if (remoteEndPoint.protocol() == tcp::v6())
        m_connectionID = "[" + m_ipAddress.to_string() + "]:" + ToString(remoteEndPoint.port());
    else
        m_connectionID = m_ipAddress.to_string() + ":" + ToString(remoteEndPoint.port());

    try
    {
        DnsResolver resolver(m_commandChannelService);
        const DnsResolver::query query(m_ipAddress.to_string(), ToString(remoteEndPoint.port()));
        DnsResolver::iterator iterator = resolver.resolve(query);
        const DnsResolver::iterator end;

        while (iterator != end)
        {
            auto endPoint = *iterator++;

            if (!endPoint.host_name().empty())
            {
                m_hostName = endPoint.host_name();
                m_connectionID = m_hostName + " (" + m_connectionID + ")";
                break;
            }
        }
    }
    catch (...)
    {   //-V565
        // DNS lookup failure is not catastrophic
    }

    if (m_hostName.empty())
        m_hostName = m_ipAddress.to_string();

    m_pingTimer.Start();
    m_stopped = false;
    ReadCommandChannel();
}

void ClientConnection::Stop()
{
    m_stopped = true;
    m_pingTimer.Stop();
    m_commandChannelSocket.shutdown(socket_base::shutdown_both);
    m_commandChannelSocket.cancel();
    m_parent->RemoveConnection(shared_from_this());
}

// All commands received from the client are handled by this thread.
void ClientConnection::ReadCommandChannel()
{
    if (!m_stopped)
        async_read(m_commandChannelSocket, buffer(m_readBuffer, Common::PayloadHeaderSize), bind(&ClientConnection::ReadPayloadHeader, this, _1, _2));
}

void ClientConnection::ReadPayloadHeader(const ErrorCode& error, uint32_t bytesTransferred)
{
    const uint32_t PacketSizeOffset = 4;

    if (m_stopped)
        return;

    // Stop cleanly, i.e., don't report, on these errors
    if (error == error::connection_aborted || error == error::connection_reset || error == error::eof)
    {
        Stop();
        return;
    }

    if (error)
    {
        stringstream messageStream;

        messageStream << "Error reading data from client \"";
        messageStream << m_connectionID;
        messageStream << "\" command channel: ";
        messageStream << SystemError(error).what();

        m_parent->DispatchErrorMessage(messageStream.str());

        Stop();
        return;
    }

    const uint32_t packetSize = EndianConverter::ToLittleEndian<uint32_t>(&m_readBuffer[0], PacketSizeOffset);

    if (packetSize > static_cast<uint32_t>(m_readBuffer.size()))
        m_readBuffer.resize(packetSize);

    // Read packet (payload body)
    // This read method is guaranteed not to return until the
    // requested size has been read or an error has occurred.
    async_read(m_commandChannelSocket, buffer(m_readBuffer, packetSize), bind(&ClientConnection::ParseCommand, this, _1, _2));
}

void ClientConnection::ParseCommand(const ErrorCode& error, uint32_t bytesTransferred)
{
    if (m_stopped)
        return;

    // Stop cleanly, i.e., don't report, on these errors
    if (error == error::connection_aborted || error == error::connection_reset || error == error::eof)
    {
        Stop();
        return;
    }

    if (error)
    {
        stringstream messageStream;

        messageStream << "Error reading data from client \"";
        messageStream << m_connectionID;
        messageStream << "\" command channel: ";
        messageStream << SystemError(error).what();

        m_parent->DispatchErrorMessage(messageStream.str());

        Stop();
        return;
    }

    try
    {
        const ClientConnectionPtr connection = shared_from_this();
        uint8_t* data = &m_readBuffer[0];
        const uint32_t command = data[0];
        data++;

        switch (command)
        {
            case ServerCommand::Subscribe:
                m_parent->HandleSubscribe(connection, data, bytesTransferred);
                break;
            case ServerCommand::Unsubscribe:
                m_parent->HandleUnsubscribe(connection);
                break;
            case ServerCommand::MetadataRefresh:
                m_parent->HandleMetadataRefresh(connection, data, bytesTransferred);
                break;
            case ServerCommand::RotateCipherKeys:
                m_parent->HandleRotateCipherKeys(connection);
                break;
            case ServerCommand::UpdateProcessingInterval:
                m_parent->HandleUpdateProcessingInterval(connection, data, bytesTransferred);
                break;
            case ServerCommand::DefineOperationalModes:
                m_parent->HandleDefineOperationalModes(connection, data, bytesTransferred);
                break;
            case ServerCommand::ConfirmNotification:
                m_parent->HandleConfirmNotification(connection, data, bytesTransferred);
                break;
            case ServerCommand::ConfirmBufferBlock:
                m_parent->HandleConfirmBufferBlock(connection, data, bytesTransferred);
                break;
            case ServerCommand::PublishCommandMeasurements:
                m_parent->HandlePublishCommandMeasurements(connection, data, bytesTransferred);
                break;
            case ServerCommand::UserCommand00:
            case ServerCommand::UserCommand01:
            case ServerCommand::UserCommand02:
            case ServerCommand::UserCommand03:
            case ServerCommand::UserCommand04:
            case ServerCommand::UserCommand05:
            case ServerCommand::UserCommand06:
            case ServerCommand::UserCommand07:
            case ServerCommand::UserCommand08:
            case ServerCommand::UserCommand09:
            case ServerCommand::UserCommand10:
            case ServerCommand::UserCommand11:
            case ServerCommand::UserCommand12:
            case ServerCommand::UserCommand13:
            case ServerCommand::UserCommand14:
            case ServerCommand::UserCommand15:
                m_parent->HandleUserCommand(connection, command, data, bytesTransferred);
                break;
            default:
            {
                stringstream messageStream;

                messageStream << "\"" << m_connectionID << "\"";
                messageStream << " sent an unrecognized server command: ";
                messageStream << ToHex(command);

                const string message = messageStream.str();
                m_parent->SendClientResponse(connection, ServerResponse::Failed, command, message);
                m_parent->DispatchErrorMessage(message);
                break;
            }
        }
    }
    catch (const std::exception& ex)
    {
        m_parent->DispatchErrorMessage("Encountered an exception while processing received client data: " + string(ex.what()));
    }

    ReadCommandChannel();
}

void ClientConnection::CommandChannelSendAsync(uint8_t* data, uint32_t offset, uint32_t length)
{
    if (!m_stopped)
        async_write(m_commandChannelSocket, buffer(&data[offset], length), bind(&ClientConnection::WriteHandler, this, _1, _2));
}

void ClientConnection::DataChannelSendAsync(uint8_t* data, uint32_t offset, uint32_t length)
{
    // TODO: Implement UDP send
    CommandChannelSendAsync(data, offset, length);
}

void ClientConnection::WriteHandler(const ErrorCode& error, uint32_t bytesTransferred)
{
    if (m_stopped)
        return;

    // Stop cleanly, i.e., don't report, on these errors
    if (error == error::connection_aborted || error == error::connection_reset || error == error::eof)
    {
        Stop();
        return;
    }

    if (error)
    {
        stringstream messageStream;

        messageStream << "Error writing data to client \"";
        messageStream << m_connectionID;
        messageStream << "\" command channel: ";
        messageStream << SystemError(error).what();

        m_parent->DispatchErrorMessage(messageStream.str());

        Stop();
    }
}

void ClientConnection::PingTimerElapsed(Timer* timer, void* userData)
{
    ClientConnection* connection = static_cast<ClientConnection*>(userData);

    if (connection == nullptr)
        return;

    if (!connection->m_stopped)
        connection->m_parent->SendClientResponse(connection->shared_from_this(), ServerResponse::NoOP, ServerCommand::Subscribe);
}

// --- DataPublisher ---

DataPublisher::DataPublisher(const TcpEndPoint& endpoint) :
    m_nodeID(NewGuid()),
    m_securityMode(SecurityMode::None),
    m_allowMetadataRefresh(true),
    m_allowNaNValueFilter(true),
    m_forceNaNValueFilter(false),
    m_cipherKeyRotationPeriod(60000),
    m_userData(nullptr),
    m_disposing(false),
    m_totalCommandChannelBytesSent(0L),
    m_totalDataChannelBytesSent(0L),
    m_totalMeasurementsSent(0L),
    m_connected(false),
    m_clientAcceptor(m_commandChannelService, endpoint)
{
    m_tableIDFields = NewSharedPtr<TableIDFields>();
    m_tableIDFields->SignalIDFieldName = "SignalID";
    m_tableIDFields->MeasurementKeyFieldName = "ID";
    m_tableIDFields->PointTagFieldName = "PointTag";

    m_callbackThread = Thread(bind(&DataPublisher::RunCallbackThread, this));
    m_commandChannelAcceptThread = Thread(bind(&DataPublisher::RunCommandChannelAcceptThread, this));
}

DataPublisher::DataPublisher(uint16_t port, bool ipV6) :
    DataPublisher(TcpEndPoint(ipV6 ? tcp::v6() : tcp::v4(), port))
{
}

DataPublisher::~DataPublisher()
{
    m_disposing = true;
}

DataPublisher::CallbackDispatcher::CallbackDispatcher() :
    Source(nullptr),
    Data(nullptr),
    Function(nullptr)
{
}

void DataPublisher::RunCallbackThread()
{
    while (true)
    {
        m_callbackQueue.WaitForData();

        if (m_disposing)
            break;

        const CallbackDispatcher dispatcher = m_callbackQueue.Dequeue();
        dispatcher.Function(dispatcher.Source, *dispatcher.Data);
    }
}

void DataPublisher::RunCommandChannelAcceptThread()
{
    StartAccept();
    m_commandChannelService.run();
}

void DataPublisher::StartAccept()
{
    const ClientConnectionPtr connection = NewSharedPtr<ClientConnection, DataPublisherPtr, IOContext&, IOContext&>(shared_from_this(), m_commandChannelService, m_dataChannelService);
    m_clientAcceptor.async_accept(connection->CommandChannelSocket(), boost::bind(&DataPublisher::AcceptConnection, this, connection, asio::placeholders::error));
}

void DataPublisher::AcceptConnection(const ClientConnectionPtr& connection, const ErrorCode& error)
{
    if (!error)
    {
        // TODO: For secured connections, validate certificate and IP information here to assign subscriberID
        m_clientConnectionsLock.lock();
        m_clientConnections.insert(connection);
        m_clientConnectionsLock.unlock();

        connection->Start();
        DispatchClientConnected(connection->GetSubscriberID(), connection->GetConnectionID());
    }

    StartAccept();
}

void DataPublisher::RemoveConnection(const ClientConnectionPtr& connection)
{
    m_clientConnectionsLock.lock();

    if (m_clientConnections.erase(connection))
        DispatchClientDisconnected(connection->GetSubscriberID(), connection->GetConnectionID());

    m_clientConnectionsLock.unlock();
}

bool DataPublisher::ParseClientSubscription(const ClientConnectionPtr& connection, const string& filterExpression, SignalIndexCachePtr& signalIndexCache)
{
    signalIndexCache = NewSharedPtr<SignalIndexCache>();
    string exceptionMessage, parsingException;
    FilterExpressionParser parser(filterExpression);
                        
    parser.SetDataSet(m_activeMetadata);
    parser.SetTableIDFields("ActiveMeasurements", m_tableIDFields);
    parser.RegisterParsingExceptionCallback([&parsingException](FilterExpressionParserPtr, const string& exception) { parsingException = exception; });

    try
    {
        parser.Evaluate();
    }
    catch (const FilterExpressionParserException& ex)
    {
        exceptionMessage = "FilterExpressionParser exception: " + string(ex.what());
    }
    catch (const ExpressionTreeException& ex)
    {
        exceptionMessage = "ExpressionTree exception: " + string(ex.what());
    }
    catch (...)
    {
        exceptionMessage = current_exception_diagnostic_information(true);
    }

    if (!exceptionMessage.empty())
    {
        if (!parsingException.empty())
            exceptionMessage += "\n" + parsingException;

        SendClientResponse(connection, ServerResponse::Failed, ServerCommand::Subscribe, exceptionMessage);
        DispatchErrorMessage(exceptionMessage);
        return false;
    }

    const DataTablePtr& activeMeasurements = m_activeMetadata->Table("ActiveMeasurements");
    const vector<DataRowPtr>& rows = parser.FilteredRows();
    const int32_t idColumn = GetColumnIndex(activeMeasurements, "ID");
    const int32_t signalIDColumn = GetColumnIndex(activeMeasurements, "SignalID");

    for (size_t i = 0; i < rows.size(); i++)
    {
        const DataRowPtr& row = rows[i];
        const Guid& signalID = row->ValueAsGuid(signalIDColumn).GetValueOrDefault();
        const vector<string> parts = Split(row->ValueAsString(idColumn).GetValueOrDefault(), ":");
        uint16_t signalIndex = 0;
        string source;
        int32_t id = 0;

        if (parts.size() == 2)
        {
            source = parts[0];
            id = stoi(parts[1]);
        }
        else
        {
            source = parts[0];
        }

        signalIndexCache->AddMeasurementKey(signalIndex++, signalID, source, id);
    }
    
    return true;
}

void DataPublisher::HandleSubscribe(const ClientConnectionPtr& connection, uint8_t* data, uint32_t length)
{
    try
    {
        if (length >= 6)
        {
            const uint8_t flags = data[0];
            int32_t index = 1;

            if ((flags & DataPacketFlags::Synchronized) > 0)
            {
                // Remotely synchronized subscriptions are currently disallowed by data publisher
                const string message = "Client request for remotely synchronized data subscription was denied. Data publisher currently does not allow for synchronized subscriptions.";
                SendClientResponse(connection, ServerResponse::Failed, ServerCommand::Subscribe, message);
                DispatchErrorMessage(message);
            }
            else
            {
                // Next 4 bytes are an integer representing the length of the connection string that follows
                const uint32_t byteLength = EndianConverter::ToBigEndian<uint32_t>(data, index);
                index += 4;

                if (byteLength > 0 && length >= byteLength + 6U)
                {
                    const bool usePayloadCompression = (connection->GetOperationalModes() & OperationalModes::CompressPayloadData) > 0;
                    const uint32_t compressionModes = connection->GetOperationalModes() & OperationalModes::CompressionModeMask;
                    const bool useCompactMeasurementFormat = (flags & DataPacketFlags::Compact) > 0;
                    const string connectionString = DecodeClientString(connection, data, index, byteLength);
                    const StringMap<string> settings = ParseKeyValuePairs(connectionString);
                    string setting;
                    bool includeTime = true;
                    bool useMillisecondResolution = false; // Default to tick resolution
                    bool isNaNFiltered = false;

                    if (TryGetValue(settings, "includeTime", setting))
                        includeTime = ParseBoolean(setting);

                    if (TryGetValue(settings, "useMillisecondResolution", setting))
                        useMillisecondResolution = ParseBoolean(setting);

                    if (TryGetValue(settings, "requestNaNValueFilter", setting))
                        isNaNFiltered = ParseBoolean(setting);

                    connection->SetUsePayloadCompression(usePayloadCompression);
                    connection->SetUseCompactMeasurementFormat(useCompactMeasurementFormat);

                    SignalIndexCachePtr signalIndexCache = nullptr;

                    // Apply subscriber filter expression and build signal index cache
                    if (TryGetValue(settings, "inputMeasurementKeys", setting))
                    {
                        if (!ParseClientSubscription(connection, setting, signalIndexCache))
                            return;
                    }

                    // Pass subscriber assembly information to connection, if defined
                    if (TryGetValue(settings, "assemblyInfo", setting))
                    {
                        connection->SetSubscriptionInfo(setting);
                        DispatchStatusMessage("Reported client subscription info: " + connection->GetSubscriptionInfo());
                    }

                    // TODO: Set up UDP data channel if client has requested this
                    if (TryGetValue(settings, "dataChannel", setting))
                    {
                    /*
                        Socket clientSocket = connection.GetCommandChannelSocket();
                        Dictionary<string, string> settings = setting.ParseKeyValuePairs();
                        IPEndPoint localEndPoint = null;
                        string networkInterface = "::0";

                        // Make sure return interface matches incoming client connection
                        if ((object)clientSocket != null)
                            localEndPoint = clientSocket.LocalEndPoint as IPEndPoint;

                        if ((object)localEndPoint != null)
                        {
                            networkInterface = localEndPoint.Address.ToString();

                            // Remove dual-stack prefix
                            if (networkInterface.StartsWith("::ffff:", true, CultureInfo.InvariantCulture))
                                networkInterface = networkInterface.Substring(7);
                        }

                        if (settings.TryGetValue("port", out setting) || settings.TryGetValue("localport", out setting))
                        {
                            if ((compressionModes & CompressionModes.TSSC) > 0)
                            {
                                // TSSC is a stateful compression algorithm which will not reliably support UDP
                                OnStatusMessage(MessageLevel.Warning, "Cannot use TSSC compression mode with UDP - special compression mode disabled");

                                // Disable TSSC compression processing
                                compressionModes &= ~CompressionModes.TSSC;
                                connection.OperationalModes &= ~OperationalModes.CompressionModeMask;
                                connection.OperationalModes |= (OperationalModes)compressionModes;
                            }

                            connection.DataChannel = new UdpServer($"Port=-1; Clients={connection.IPAddress}:{int.Parse(setting)}; interface={networkInterface}");
                            connection.DataChannel.Start();
                        }
                    */
                    }

                    // Send updated signal index cache to client with validated rights of the selected input measurement keys
                    vector<uint8_t> serializedSignalIndexCache;
                    SerializeSignalIndexCache(connection, signalIndexCache, serializedSignalIndexCache);
                    SendClientResponse(connection, ServerResponse::UpdateSignalIndexCache, ServerCommand::Subscribe, serializedSignalIndexCache);
                    connection->SetSignalIndexCache(signalIndexCache);

                    int32_t signalCount = 0;

                    if (signalIndexCache != nullptr)
                        signalCount = signalIndexCache->Count();

                    const string message = "Client subscribed as " + string(useCompactMeasurementFormat ? "" : "non-") + "compact unsynchronized with " + ToString(signalCount) + " signals.";

                    connection->SetIsSubscribed(true);
                    SendClientResponse(connection, ServerResponse::Succeeded, ServerCommand::Subscribe, message);
                    DispatchStatusMessage(message);
                }
                else
                {
                    const string message = byteLength > 0 ?
                        "Not enough buffer was provided to parse client data subscription." :
                        "Cannot initialize client data subscription without a connection string.";

                    SendClientResponse(connection, ServerResponse::Failed, ServerCommand::Subscribe, message);
                    DispatchErrorMessage(message);
                }            
            }            
        }
        else
        {
            const string message = "Not enough buffer was provided to parse client data subscription.";
            SendClientResponse(connection, ServerResponse::Failed, ServerCommand::Subscribe, message);
            DispatchErrorMessage(message);
        }
    }
    catch (const std::exception& ex)
    {
        const string message = "Failed to process client data subscription due to exception: " + string(ex.what());
        SendClientResponse(connection, ServerResponse::Failed, ServerCommand::Subscribe, message);
        DispatchErrorMessage(message);
    }
}

void DataPublisher::HandleUnsubscribe(const ClientConnectionPtr& connection)
{
    connection->SetIsSubscribed(false);
}

void DataPublisher::HandleMetadataRefresh(const ClientConnectionPtr& connection, uint8_t* data, uint32_t length)
{
    // Ensure that the subscriber is allowed to request meta-data
    if (!m_allowMetadataRefresh)
        throw PublisherException("Meta-data refresh has been disallowed by the DataPublisher.");

    DispatchStatusMessage("Received meta-data refresh request from " + connection->GetConnectionID() + ", preparing response...");

    StringMap<ExpressionTreePtr> filterExpressions;
    const DateTime startTime = UtcNow(); //-V821

    try
    {
        uint32_t index = 0;

        // Note that these client provided meta-data filter expressions are applied only to the
        // in-memory DataSet and therefore are not subject to SQL injection attacks
        if (length > 4)
        {
            const uint32_t responseLength = EndianConverter::ToBigEndian<uint32_t>(data, index);
            index += 4;

            if (length >= responseLength + 4)
            {
                const string metadataFilters = DecodeClientString(connection, data, index, responseLength);
                const vector<ExpressionTreePtr> expressions = FilterExpressionParser::GenerateExpressionTrees(m_allMetadata, "MeasurementDetail", metadataFilters);

                // Go through each subscriber specified filter expressions and add it to dictionary
                for (const auto& expression : expressions)
                    filterExpressions[expression->Table()->Name()] = expression;
            }
        }
    }
    catch (const std::exception& ex)
    {
        DispatchErrorMessage("Failed to parse subscriber provided meta-data filter expressions: " + string(ex.what()));
    }

    try
    {
        const DataSetPtr metadata = FilterClientMetadata(connection, filterExpressions);
        const vector<uint8_t> serializedMetadata = SerializeMetadata(connection, metadata);
        vector<DataTablePtr> tables = metadata->Tables();
        uint64_t rowCount = 0;

        for (size_t i = 0; i < tables.size(); i++)
            rowCount += tables[i]->RowCount();

        if (rowCount > 0)
        {
            const TimeSpan elapsedTime = UtcNow() - startTime;
            DispatchStatusMessage(ToString(rowCount) + " records spanning " + ToString(tables.size()) + " tables of meta-data prepared in " + ToString(elapsedTime) + ", sending response to " + connection->GetConnectionID() + "...");
        }
        else
        {
            DispatchStatusMessage("No meta-data is available" + string(filterExpressions.empty() ? "" : " due to user applied meta-data filters") + ", sending an empty response to " + connection->GetConnectionID() + "...");
        }

        SendClientResponse(connection, ServerResponse::Succeeded, ServerCommand::MetadataRefresh, serializedMetadata);
    }
    catch (const std::exception& ex)
    {
        const string message = "Failed to transfer meta-data due to exception: " + string(ex.what());
        SendClientResponse(connection, ServerResponse::Failed, ServerCommand::MetadataRefresh, message);
        DispatchErrorMessage(message);
    }
}

void DataPublisher::HandleRotateCipherKeys(const ClientConnectionPtr& connection)
{
}

void DataPublisher::HandleUpdateProcessingInterval(const ClientConnectionPtr& connection, uint8_t* data, uint32_t length)
{
}

void DataPublisher::HandleDefineOperationalModes(const ClientConnectionPtr& connection, uint8_t* data, uint32_t length)
{
    if (length < 4)
        return;

    const uint32_t operationalModes = EndianConverter::Default.ToBigEndian<uint32_t>(data, 0);

    if ((operationalModes & OperationalModes::VersionMask) != 0U)
        DispatchStatusMessage("Protocol version not supported. Operational modes may not be set correctly for client \"" + connection->GetConnectionID() + "\".");

    connection->SetOperationalModes(operationalModes);
}

void DataPublisher::HandleConfirmNotification(const ClientConnectionPtr& connection, uint8_t* data, uint32_t length)
{
}

void DataPublisher::HandleConfirmBufferBlock(const ClientConnectionPtr& connection, uint8_t* data, uint32_t length)
{
}

void DataPublisher::HandlePublishCommandMeasurements(const ClientConnectionPtr& connection, uint8_t* data, uint32_t length)
{
}

void DataPublisher::HandleUserCommand(const ClientConnectionPtr& connection, uint8_t command, uint8_t* data, uint32_t length)
{
}

void DataPublisher::Dispatch(const DispatcherFunction& function)
{
    Dispatch(function, nullptr, 0, 0);
}

void DataPublisher::Dispatch(const DispatcherFunction& function, const uint8_t* data, uint32_t offset, uint32_t length)
{
    CallbackDispatcher dispatcher;
    SharedPtr<vector<uint8_t>> dataVector = NewSharedPtr<vector<uint8_t>>();

    dataVector->resize(length);

    if (data != nullptr)
    {
        for (uint32_t i = 0; i < length; ++i)
            dataVector->at(i) = data[offset + i];
    }

    dispatcher.Source = this;
    dispatcher.Data = dataVector;
    dispatcher.Function = function;

    m_callbackQueue.Enqueue(dispatcher);
}

void DataPublisher::DispatchStatusMessage(const string& message)
{
    const uint32_t messageSize = (message.size() + 1) * sizeof(char);
    Dispatch(&StatusMessageDispatcher, reinterpret_cast<const uint8_t*>(message.c_str()), 0, messageSize);
}

void DataPublisher::DispatchErrorMessage(const string& message)
{
    const uint32_t messageSize = (message.size() + 1) * sizeof(char);
    Dispatch(&ErrorMessageDispatcher, reinterpret_cast<const uint8_t*>(message.c_str()), 0, messageSize);
}

void DataPublisher::DispatchClientConnected(const GSF::Guid& subscriberID, const string& connectionID)
{
    ClientConnectionInfo* data = new ClientConnectionInfo(subscriberID, connectionID);
    Dispatch(&ClientConnectedDispatcher, reinterpret_cast<uint8_t*>(&data), 0, sizeof(ClientConnectionInfo**));
}

void DataPublisher::DispatchClientDisconnected(const GSF::Guid& subscriberID, const std::string& connectionID)
{
    ClientConnectionInfo* data = new ClientConnectionInfo(subscriberID, connectionID);
    Dispatch(&ClientDisconnectedDispatcher, reinterpret_cast<uint8_t*>(&data), 0, sizeof(ClientConnectionInfo**));
}

// Dispatcher function for status messages. Decodes the message and provides it to the user via the status message callback.
void DataPublisher::StatusMessageDispatcher(DataPublisher* source, const vector<uint8_t>& buffer)
{
    if (source == nullptr)
        return;

    const MessageCallback statusMessageCallback = source->m_statusMessageCallback;

    if (statusMessageCallback != nullptr)
        statusMessageCallback(source, reinterpret_cast<const char*>(&buffer[0]));
}

// Dispatcher function for error messages. Decodes the message and provides it to the user via the error message callback.
void DataPublisher::ErrorMessageDispatcher(DataPublisher* source, const vector<uint8_t>& buffer)
{
    if (source == nullptr)
        return;

    const MessageCallback errorMessageCallback = source->m_errorMessageCallback;

    if (errorMessageCallback != nullptr)
        errorMessageCallback(source, reinterpret_cast<const char*>(&buffer[0]));
}

void DataPublisher::ClientConnectedDispatcher(DataPublisher* source, const vector<uint8_t>& buffer)
{
    ClientConnectionInfo* data = *reinterpret_cast<ClientConnectionInfo**>(const_cast<uint8_t*>(&buffer[0]));

    if (source != nullptr)
    {
        const ClientConnectionCallback clientConnectedCallback = source->m_clientConnectedCallback;

        if (clientConnectedCallback != nullptr)
            clientConnectedCallback(source, data->SubscriberID, data->ConnectionID);
    }

    delete data;
}

void DataPublisher::ClientDisconnectedDispatcher(DataPublisher* source, const std::vector<uint8_t>& buffer)
{
    ClientConnectionInfo* data = *reinterpret_cast<ClientConnectionInfo**>(const_cast<uint8_t*>(&buffer[0]));

    if (source != nullptr)
    {
        const ClientConnectionCallback clientDisconnectedCallback = source->m_clientDisconnectedCallback;

        if (clientDisconnectedCallback != nullptr)
            clientDisconnectedCallback(source, data->SubscriberID, data->ConnectionID);
    }

    delete data;
}

void DataPublisher::SerializeSignalIndexCache(const ClientConnectionPtr& connection, const SignalIndexCachePtr& signalIndexCache, vector<uint8_t>& buffer)
{
    // TODO: Serialize signal index cache
}

string DataPublisher::DecodeClientString(const ClientConnectionPtr& connection, const uint8_t* data, uint32_t offset, uint32_t length) const
{
    uint32_t encoding = OperationalEncoding::UTF8;
    bool swapBytes = EndianConverter::IsLittleEndian();

    if (connection != nullptr)
        encoding = connection->GetEncoding();

    switch (encoding)
    {
        case OperationalEncoding::UTF8:
            return string(reinterpret_cast<const char*>(data + offset), length / sizeof(char));
        case OperationalEncoding::Unicode:
        case OperationalEncoding::ANSI:
            swapBytes = !swapBytes;
        case OperationalEncoding::BigEndianUnicode:
        {
            wstring value{};
            value.reserve(length / sizeof(wchar_t));

            for (size_t i = 0; i < length; i += sizeof(wchar_t))
            {
                if (swapBytes)
                    value.append(1, EndianConverter::ToLittleEndian<wchar_t>(data, offset + i));
                else
                    value.append(1, *reinterpret_cast<const wchar_t*>(data + offset + i));
            }

            return ToUTF8(value);
        }
        default:
            throw PublisherException("Encountered unexpected operational encoding " + ToHex(encoding));
    }
}

vector<uint8_t> DataPublisher::EncodeClientString(const ClientConnectionPtr& connection, const std::string& value) const
{
    uint32_t encoding = OperationalEncoding::UTF8;
    bool swapBytes = EndianConverter::IsLittleEndian();

    if (connection != nullptr)
        encoding = connection->GetEncoding();

    vector<uint8_t> result{};

    switch (encoding)
    {
        case OperationalEncoding::UTF8:
            result.reserve(value.size() * sizeof(char));
            result.assign(value.begin(), value.end());
            break;
        case OperationalEncoding::Unicode:
        case OperationalEncoding::ANSI:
            swapBytes = !swapBytes;
        case OperationalEncoding::BigEndianUnicode:
        {
            wstring utf16 = ToUTF16(value);            
            const int32_t size = utf16.size() * sizeof(wchar_t);
            const uint8_t* data = reinterpret_cast<const uint8_t*>(&utf16[0]);

            result.reserve(size);

            for (int32_t i = 0; i < size; i += sizeof(wchar_t))
            {
                if (swapBytes)
                {
                    result.push_back(data[i + 1]);
                    result.push_back(data[i]);
                }
                else
                {
                    result.push_back(data[i]);
                    result.push_back(data[i + 1]);
                }
            }

            break;
        }
        default:
            throw PublisherException("Encountered unexpected operational encoding " + ToHex(encoding));
    }

    return result;
}

DataSetPtr DataPublisher::FilterClientMetadata(const ClientConnectionPtr& connection, const StringMap<ExpressionTreePtr>& filterExpressions) const
{
    if (filterExpressions.empty())
        return m_allMetadata;

    DataSetPtr dataSet = NewSharedPtr<DataSet>();
    vector<DataTablePtr> tables = m_allMetadata->Tables();

    for (size_t i = 0; i < tables.size(); i++)
    {
        const DataTablePtr table = tables[i];
        DataTablePtr filteredTable = dataSet->CreateTable(table->Name());
        ExpressionTreePtr expression;

        for (int32_t j = 0; j < table->ColumnCount(); j++)
            filteredTable->AddColumn(filteredTable->CloneColumn(table->Column(j)));

        if (TryGetValue<ExpressionTreePtr>(filterExpressions, table->Name(), expression, nullptr))
        {
            vector<DataRowPtr> matchedRows = FilterExpressionParser::Select(expression);

            for (size_t j = 0; j < matchedRows.size(); j++)
                filteredTable->AddRow(filteredTable->CloneRow(matchedRows[j]));
        }
        else
        {
            for (int32_t j = 0; j < table->RowCount(); j++)
                filteredTable->AddRow(filteredTable->CloneRow(table->Row(j)));
        }

        dataSet->AddOrUpdateTable(filteredTable);
    }

    return dataSet;
}

vector<uint8_t> DataPublisher::SerializeMetadata(const ClientConnectionPtr& connection, const DataSetPtr& metadata) const
{
    vector<uint8_t> serializedMetadata;

    if (connection != nullptr)
    {
        const uint32_t operationalModes = connection->GetOperationalModes();
        const uint32_t compressionModes = operationalModes & OperationalModes::CompressionModeMask;
        const bool useCommonSerializationFormat = (operationalModes & OperationalModes::UseCommonSerializationFormat) > 0;
        const bool compressMetadata = (operationalModes & OperationalModes::CompressMetadata) > 0;

        if (!useCommonSerializationFormat)
            throw PublisherException("DataPublisher only supports common serialization format");

        metadata->WriteXml(serializedMetadata);

        if (compressMetadata && (compressionModes & CompressionModes::GZip) > 0)
        {
            const MemoryStream metadataStream(serializedMetadata);
            StreamBuffer streamBuffer;

            streamBuffer.push(GZipCompressor());
            streamBuffer.push(metadataStream);

            vector<uint8_t> compressed;
            CopyStream(&streamBuffer, compressed);

            return compressed;
        }
    }

    return serializedMetadata;
}

bool DataPublisher::SendClientResponse(const ClientConnectionPtr& connection, uint8_t responseCode, uint8_t commandCode, const std::string& message)
{
    return SendClientResponse(connection, responseCode, commandCode, EncodeClientString(connection, message));
}

bool DataPublisher::SendClientResponse(const ClientConnectionPtr& connection, uint8_t responseCode, uint8_t commandCode, const std::vector<uint8_t>& data)
{
    bool success = false;

    try
    {
        const bool dataPacketResponse = responseCode == ServerResponse::DataPacket;
        const bool useDataChannel = dataPacketResponse || responseCode == ServerResponse::BufferBlock;
        const uint32_t packetSize = data.size() + 6;
        vector<uint8_t> buffer {};

        buffer.reserve(Common::PayloadHeaderSize + packetSize);

        // Add command payload alignment header (deprecated)
        buffer.push_back(0xAA);
        buffer.push_back(0xBB);
        buffer.push_back(0xCC);
        buffer.push_back(0xDD);

        EndianConverter::WriteLittleEndianBytes(buffer, packetSize);

        // Add response code
        buffer.push_back(responseCode);

        // Add original in response to command code
        buffer.push_back(commandCode);

        if (data.empty())
        {
            // Add zero sized data buffer to response packet
            buffer.push_back(0);
            buffer.push_back(0);
            buffer.push_back(0);
            buffer.push_back(0);
        }
        else
        {
            if (dataPacketResponse && connection->CipherKeysDefined())
            {
                // TODO: Implement UDP AES data packet encryption
                //// Get a local copy of volatile keyIVs and cipher index since these can change at any time
                //byte[][][] keyIVs = connection.KeyIVs;
                //int cipherIndex = connection.CipherIndex;

                //// Reserve space for size of data buffer to go into response packet
                //workingBuffer.Write(ZeroLengthBytes, 0, 4);

                //// Get data packet flags
                //DataPacketFlags flags = (DataPacketFlags)data[0];

                //// Encode current cipher index into data packet flags
                //if (cipherIndex > 0)
                //    flags |= DataPacketFlags.CipherIndex;

                //// Write data packet flags into response packet
                //workingBuffer.WriteByte((byte)flags);

                //// Copy source data payload into a memory stream
                //MemoryStream sourceData = new MemoryStream(data, 1, data.Length - 1);

                //// Encrypt payload portion of data packet and copy into the response packet
                //Common.SymmetricAlgorithm.Encrypt(sourceData, workingBuffer, keyIVs[cipherIndex][0], keyIVs[cipherIndex][1]);

                //// Calculate length of encrypted data payload
                //int payloadLength = (int)workingBuffer.Length - 6;

                //// Move the response packet position back to the packet size reservation
                //workingBuffer.Seek(2, SeekOrigin.Begin);

                //// Add the actual size of payload length to response packet
                //workingBuffer.Write(BigEndian.GetBytes(payloadLength), 0, 4);
            }
            else
            {
                // Add size of data buffer to response packet
                EndianConverter::WriteBigEndianBytes(buffer, static_cast<int32_t>(data.size()));

                // Write data buffer
                for (size_t i = 0; i < data.size(); i++)
                    buffer.push_back(data[i]);
            }

            // TODO: Publish packet on UDP
            //// Data packets and buffer blocks can be published on a UDP data channel, so check for this...
            //if (useDataChannel)
            //    publishChannel = m_clientPublicationChannels.GetOrAdd(clientID, id => (object)connection != null ? connection.PublishChannel : m_commandChannel);
            //else
            //    publishChannel = m_commandChannel;

            //// Send response packet
            //if ((object)publishChannel != null && publishChannel.CurrentState == ServerState.Running)
            //{
            //    if (publishChannel is UdpServer)
            //        publishChannel.MulticastAsync(buffer, 0, buffer.size());
            //    else
            //        publishChannel.SendToAsync(connection, buffer, 0, buffer.size());

            //}

            connection->CommandChannelSendAsync(buffer.data(), 0, buffer.size());
            m_totalCommandChannelBytesSent += buffer.size();
            success = true;
        }
    }
    catch (const std::exception& ex)
    {
        DispatchErrorMessage(ex.what());
    }

    return success;
}

void DataPublisher::DefineMetadata(const vector<DeviceMetadataPtr>& deviceMetadata, const vector<MeasurementMetadataPtr>& measurementMetadata, const vector<PhasorMetadataPtr>& phasorMetadata, const int32_t versionNumber)
{
    typedef unordered_map<uint16_t, char> PhasorTypeMap;
    typedef SharedPtr<PhasorTypeMap> PhasorTypeMapPtr;
    const PhasorTypeMapPtr nullPhasorTypeMap = nullptr;

    // Load meta-data schema
    const DataSetPtr metadata = DataSet::FromXml(MetadataSchema, MetadataSchemaLength);
    const DataTablePtr& deviceDetail = metadata->Table("DeviceDetail");
    const DataTablePtr& measurementDetail = metadata->Table("MeasurementDetail");
    const DataTablePtr& phasorDetail = metadata->Table("PhasorDetail");
    const DataTablePtr& schemaVersion = metadata->Table("SchemaVersion");

    StringMap<PhasorTypeMapPtr> phasorTypes;
    PhasorTypeMapPtr phasors;

    if (deviceDetail != nullptr)
    {
        const int32_t nodeID = GetColumnIndex(deviceDetail, "NodeID");
        const int32_t uniqueID = GetColumnIndex(deviceDetail, "UniqueID");
        const int32_t isConcentrator = GetColumnIndex(deviceDetail, "IsConcentrator");
        const int32_t acronym = GetColumnIndex(deviceDetail, "Acronym");
        const int32_t name = GetColumnIndex(deviceDetail, "Name");
        const int32_t accessID = GetColumnIndex(deviceDetail, "AccessID");
        const int32_t parentAcronym = GetColumnIndex(deviceDetail, "ParentAcronym");
        const int32_t protocolName = GetColumnIndex(deviceDetail, "ProtocolName");
        const int32_t framesPerSecond = GetColumnIndex(deviceDetail, "FramesPerSecond");
        const int32_t companyAcronym = GetColumnIndex(deviceDetail, "CompanyAcronym");
        const int32_t vendorAcronym = GetColumnIndex(deviceDetail, "VendorAcronym");
        const int32_t vendorDeviceName = GetColumnIndex(deviceDetail, "VendorDeviceAcronym");
        const int32_t longitude = GetColumnIndex(deviceDetail, "Longitude");
        const int32_t latitude = GetColumnIndex(deviceDetail, "Latitude");
        const int32_t enabled = GetColumnIndex(deviceDetail, "Enabled");
        const int32_t updatedOn = GetColumnIndex(deviceDetail, "UpdatedOn");

        for (size_t i = 0; i < deviceMetadata.size(); i++)
        {
            const DeviceMetadataPtr device = deviceMetadata[i];

            if (device == nullptr)
                continue;

            DataRowPtr row = deviceDetail->CreateRow();

            row->SetGuidValue(nodeID, m_nodeID);
            row->SetGuidValue(uniqueID, device->UniqueID);
            row->SetBooleanValue(isConcentrator, device->ParentAcronym.empty());
            row->SetStringValue(acronym, device->Acronym);
            row->SetStringValue(name, device->Name);
            row->SetInt32Value(accessID, device->AccessID);
            row->SetStringValue(parentAcronym, device->ParentAcronym);
            row->SetStringValue(protocolName, device->ProtocolName);
            row->SetInt32Value(framesPerSecond, device->FramesPerSecond);
            row->SetStringValue(companyAcronym, device->CompanyAcronym);
            row->SetStringValue(vendorAcronym, device->VendorAcronym);
            row->SetStringValue(vendorDeviceName, device->VendorDeviceName);
            row->SetDecimalValue(longitude, decimal_t(device->Longitude));
            row->SetDecimalValue(latitude, decimal_t(device->Latitude));
            row->SetBooleanValue(enabled, true);
            row->SetDateTimeValue(updatedOn, device->UpdatedOn);

            deviceDetail->AddRow(row);
        }
    }

    if (phasorDetail != nullptr)
    {
        const int32_t id = GetColumnIndex(phasorDetail, "ID");
        const int32_t deviceAcronym = GetColumnIndex(phasorDetail, "DeviceAcronym");
        const int32_t label = GetColumnIndex(phasorDetail, "Label");
        const int32_t type = GetColumnIndex(phasorDetail, "Type");
        const int32_t phase = GetColumnIndex(phasorDetail, "Phase");
        const int32_t sourceIndex = GetColumnIndex(phasorDetail, "SourceIndex");
        const int32_t updatedOn = GetColumnIndex(phasorDetail, "UpdatedOn");

        for (size_t i = 0; i < phasorMetadata.size(); i++)
        {
            const PhasorMetadataPtr phasor = phasorMetadata[i];

            if (phasor == nullptr)
                continue;

            DataRowPtr row = phasorDetail->CreateRow();

            row->SetInt32Value(id, static_cast<int32_t>(i));
            row->SetStringValue(deviceAcronym, phasor->DeviceAcronym);
            row->SetStringValue(label, phasor->Label);
            row->SetStringValue(type, phasor->Type);
            row->SetStringValue(phase, phasor->Phase);
            row->SetInt32Value(sourceIndex, phasor->SourceIndex);
            row->SetDateTimeValue(updatedOn, phasor->UpdatedOn);

            phasorDetail->AddRow(row);

            // Track phasor information related to device for measurement signal type derivation later
            if (!TryGetValue(phasorTypes, phasor->DeviceAcronym, phasors, nullPhasorTypeMap))
            {
                phasors = NewSharedPtr<PhasorTypeMap>();
                phasorTypes[phasor->DeviceAcronym] = phasors;
            }

            phasors->at(phasor->SourceIndex) = phasor->Type.empty() ? 'I' : phasor->Type[0];
        }
    }

    if (measurementDetail != nullptr)
    {
        const int32_t deviceAcronym = GetColumnIndex(measurementDetail, "DeviceAcronym");
        const int32_t id = GetColumnIndex(measurementDetail, "ID");
        const int32_t signalID = GetColumnIndex(measurementDetail, "SignalID");
        const int32_t pointTag = GetColumnIndex(measurementDetail, "PointTag");
        const int32_t signalReference = GetColumnIndex(measurementDetail, "SignalReference");
        const int32_t signalAcronym = GetColumnIndex(measurementDetail, "SignalAcronym");
        const int32_t phasorSourceIndex = GetColumnIndex(measurementDetail, "PhasorSourceIndex");
        const int32_t description = GetColumnIndex(measurementDetail, "Description");
        const int32_t internal = GetColumnIndex(measurementDetail, "Internal");
        const int32_t enabled = GetColumnIndex(measurementDetail, "Enabled");
        const int32_t updatedOn = GetColumnIndex(measurementDetail, "UpdatedOn");
        char phasorType = 'I';

        for (size_t i = 0; i < measurementMetadata.size(); i++)
        {
            const MeasurementMetadataPtr measurement = measurementMetadata[i];
            DataRowPtr row = measurementDetail->CreateRow();

            row->SetStringValue(deviceAcronym, measurement->DeviceAcronym);
            row->SetStringValue(id, measurement->ID);
            row->SetGuidValue(signalID, measurement->SignalID);
            row->SetStringValue(pointTag, measurement->PointTag);
            row->SetStringValue(signalReference, ToString(measurement->Reference));

            if (TryGetValue(phasorTypes, measurement->DeviceAcronym, phasors, nullPhasorTypeMap))
                TryGetValue(*phasors, measurement->PhasorSourceIndex, phasorType, 'I');

            row->SetStringValue(signalAcronym, GetSignalTypeAcronym(measurement->Reference.Kind, phasorType));
            row->SetInt32Value(phasorSourceIndex, measurement->PhasorSourceIndex);
            row->SetStringValue(description, measurement->Description);
            row->SetBooleanValue(internal, true);
            row->SetBooleanValue(enabled, true);
            row->SetDateTimeValue(updatedOn, measurement->UpdatedOn);

            measurementDetail->AddRow(row);
        }
    }

    if (schemaVersion != nullptr)
    {
        DataRowPtr row = schemaVersion->CreateRow();
        row->SetInt32Value("VersionNumber", versionNumber);
        schemaVersion->AddRow(row);
    }

    DefineMetadata(metadata);
}

void DataPublisher::DefineMetadata(const DataSetPtr& metadata)
{
    m_allMetadata = metadata;

    // Create device data map used to build a flatter meta-data view used for easier client filtering
    struct DeviceData
    {
        int32_t DeviceID{};
        int32_t FramesPerSecond{};
        string Company;
        string Protocol;
        string ProtocolType;
        decimal_t Longitude;
        decimal_t Latitude;
    };

    typedef SharedPtr<DeviceData> DeviceDataPtr;
    const DeviceDataPtr nullDeviceData = nullptr;

    const DataTablePtr& deviceDetail = metadata->Table("DeviceDetail");
    StringMap<DeviceDataPtr> deviceData;

    if (deviceDetail != nullptr)
    {
        const int32_t name = GetColumnIndex(deviceDetail, "Name");
        const int32_t protocolName = GetColumnIndex(deviceDetail, "ProtocolName");
        const int32_t framesPerSecond = GetColumnIndex(deviceDetail, "FramesPerSecond");
        const int32_t companyAcronym = GetColumnIndex(deviceDetail, "CompanyAcronym");
        const int32_t longitude = GetColumnIndex(deviceDetail, "Longitude");
        const int32_t latitude = GetColumnIndex(deviceDetail, "Latitude");

        for (int32_t i = 0; i < deviceDetail->RowCount(); i++)
        {
            const DataRowPtr& row = deviceDetail->Row(i);
            const DeviceDataPtr device = NewSharedPtr<DeviceData>();

            device->DeviceID = i;
            device->FramesPerSecond = row->ValueAsInt32(framesPerSecond).GetValueOrDefault();
            device->Company = row->ValueAsString(companyAcronym).GetValueOrDefault();
            device->Protocol = row->ValueAsString(protocolName).GetValueOrDefault();
            device->ProtocolType = GetProtocolType(device->Protocol);
            device->Longitude = row->ValueAsDecimal(longitude).GetValueOrDefault();
            device->Latitude = row->ValueAsDecimal(latitude).GetValueOrDefault();

            string deviceName = row->ValueAsString(name).GetValueOrDefault();

            if (!deviceName.empty())
                deviceData[deviceName] = device;
        }
    }

    // Create phasor data map used to build a flatter meta-data view used for easier client filtering
    struct PhasorData
    {
        int32_t PhasorID{};
        string PhasorType;
        string Phase;
    };

    typedef SharedPtr<PhasorData> PhasorDataPtr;
    const PhasorDataPtr nullPhasorData = nullptr;

    typedef unordered_map<int32_t, PhasorDataPtr> PhasorDataMap;
    typedef SharedPtr<PhasorDataMap> PhasorDataMapPtr;
    const PhasorDataMapPtr nullPhasorDataMap = nullptr;

    const DataTablePtr& phasorDetail = metadata->Table("PhasorDetail");
    StringMap<PhasorDataMapPtr> phasorData;

    if (phasorDetail != nullptr)
    {
        const int32_t id = GetColumnIndex(phasorDetail, "ID");
        const int32_t deviceAcronym = GetColumnIndex(phasorDetail, "DeviceAcronym");
        const int32_t type = GetColumnIndex(phasorDetail, "Type");
        const int32_t phase = GetColumnIndex(phasorDetail, "Phase");
        const int32_t sourceIndex = GetColumnIndex(phasorDetail, "SourceIndex");
        
        for (int32_t i = 0; i < phasorDetail->RowCount(); i++)
        {
            const DataRowPtr& row = phasorDetail->Row(i);
            
            string deviceName = row->ValueAsString(deviceAcronym).GetValueOrDefault();

            if (deviceName.empty())
                continue;

            PhasorDataMapPtr phasorMap;
            const PhasorDataPtr phasor = NewSharedPtr<PhasorData>();

            phasor->PhasorID = row->ValueAsInt32(id).GetValueOrDefault();
            phasor->PhasorType = row->ValueAsString(type).GetValueOrDefault();
            phasor->Phase = row->ValueAsString(phase).GetValueOrDefault();

            if (!TryGetValue(phasorData, deviceName, phasorMap, nullPhasorDataMap))
            {
                phasorMap = NewSharedPtr<PhasorDataMap>();
                phasorData[deviceName] = phasorMap;
            }

            phasorMap->at(row->ValueAsInt32(sourceIndex).GetValueOrDefault()) = phasor;
        }
    }

    // Load active meta-data measurements schema
    m_activeMetadata = DataSet::FromXml(ActiveMeasurementsSchema, ActiveMeasurementsSchemaLength);

    // Build active meta-data measurements from all meta-data
    const DataTablePtr& measurementDetail = metadata->Table("MeasurementDetail");
    const DataTablePtr& activeMeasurements = m_activeMetadata->Table("ActiveMeasurements");

    if (measurementDetail != nullptr && activeMeasurements != nullptr)
    {
        // Lookup column indices for measurement detail table
        const int32_t md_deviceAcronym = GetColumnIndex(measurementDetail, "DeviceAcronym");
        const int32_t md_id = GetColumnIndex(measurementDetail, "ID");
        const int32_t md_signalID = GetColumnIndex(measurementDetail, "SignalID");
        const int32_t md_pointTag = GetColumnIndex(measurementDetail, "PointTag");
        const int32_t md_signalReference = GetColumnIndex(measurementDetail, "SignalReference");
        const int32_t md_signalAcronym = GetColumnIndex(measurementDetail, "SignalAcronym");
        const int32_t md_phasorSourceIndex = GetColumnIndex(measurementDetail, "PhasorSourceIndex");
        const int32_t md_description = GetColumnIndex(measurementDetail, "Description");
        const int32_t md_internal = GetColumnIndex(measurementDetail, "Internal");
        const int32_t md_enabled = GetColumnIndex(measurementDetail, "Enabled");
        const int32_t md_updatedOn = GetColumnIndex(measurementDetail, "UpdatedOn");

        // Lookup column indices for active measurements table
        const int32_t am_sourceNodeID = GetColumnIndex(activeMeasurements, "SourceNodeID");
        const int32_t am_id = GetColumnIndex(activeMeasurements, "ID");
        const int32_t am_signalID = GetColumnIndex(activeMeasurements, "SignalID");
        const int32_t am_pointTag = GetColumnIndex(activeMeasurements, "PointTag");
        const int32_t am_signalReference = GetColumnIndex(activeMeasurements, "SignalReference");
        const int32_t am_internal = GetColumnIndex(activeMeasurements, "Internal");
        const int32_t am_subscribed = GetColumnIndex(activeMeasurements, "Subscribed");
        const int32_t am_device = GetColumnIndex(activeMeasurements, "Device");
        const int32_t am_deviceID = GetColumnIndex(activeMeasurements, "DeviceID");
        const int32_t am_framesPerSecond = GetColumnIndex(activeMeasurements, "FramesPerSecond");
        const int32_t am_protocol = GetColumnIndex(activeMeasurements, "Protocol");
        const int32_t am_protocolType = GetColumnIndex(activeMeasurements, "ProtocolType");
        const int32_t am_signalType = GetColumnIndex(activeMeasurements, "SignalType");
        const int32_t am_engineeringUnits = GetColumnIndex(activeMeasurements, "EngineeringUnits");
        const int32_t am_phasorID = GetColumnIndex(activeMeasurements, "PhasorID");
        const int32_t am_phasorType = GetColumnIndex(activeMeasurements, "PhasorType");
        const int32_t am_phase = GetColumnIndex(activeMeasurements, "Phase");
        const int32_t am_adder = GetColumnIndex(activeMeasurements, "Adder");
        const int32_t am_multiplier = GetColumnIndex(activeMeasurements, "Multiplier");
        const int32_t am_company = GetColumnIndex(activeMeasurements, "Company");
        const int32_t am_longitude = GetColumnIndex(activeMeasurements, "Longitude");
        const int32_t am_latitude = GetColumnIndex(activeMeasurements, "Latitude");
        const int32_t am_description = GetColumnIndex(activeMeasurements, "Description");
        const int32_t am_updatedOn = GetColumnIndex(activeMeasurements, "UpdatedOn");

        for (int32_t i = 0; i < measurementDetail->RowCount(); i++)
        {
            const DataRowPtr& md_row = measurementDetail->Row(i);

            if (!md_row->ValueAsBoolean(md_enabled).GetValueOrDefault())
                continue;
            
            DataRowPtr am_row = activeMeasurements->CreateRow();

            am_row->SetGuidValue(am_sourceNodeID, m_nodeID);
            am_row->SetStringValue(am_id, md_row->ValueAsString(md_id));
            am_row->SetGuidValue(am_signalID, md_row->ValueAsGuid(md_signalID));
            am_row->SetStringValue(am_pointTag, md_row->ValueAsString(md_pointTag));
            am_row->SetStringValue(am_signalReference, md_row->ValueAsString(md_signalReference));
            am_row->SetInt32Value(am_internal, md_row->ValueAsInt32(md_internal));
            am_row->SetBooleanValue(am_subscribed, false);
            am_row->SetStringValue(am_description, md_row->ValueAsString(md_description));
            am_row->SetDoubleValue(am_adder, 0.0);
            am_row->SetDoubleValue(am_multiplier, 1.0);
            am_row->SetDateTimeValue(am_updatedOn, md_row->ValueAsDateTime(md_updatedOn));

            string signalType = md_row->ValueAsString(md_signalAcronym).GetValueOrDefault();

            if (signalType.empty())
                signalType = "CALC";

            am_row->SetStringValue(am_signalType, signalType);
            am_row->SetStringValue(am_engineeringUnits, GetEngineeringUnits(signalType));

            string deviceName = md_row->ValueAsString(md_deviceAcronym).GetValueOrDefault();

            if (deviceName.empty())
            {
                // Set any default values when measurement is not associated with a device
                am_row->SetInt32Value(am_framesPerSecond, 30);
            }
            else
            {
                am_row->SetStringValue(am_device, deviceName);

                DeviceDataPtr device;

                // Lookup associated device record
                if (TryGetValue(deviceData, deviceName, device, nullDeviceData))
                {
                    am_row->SetInt32Value(am_deviceID, device->DeviceID);
                    am_row->SetInt32Value(am_framesPerSecond, device->FramesPerSecond);
                    am_row->SetStringValue(am_company, device->Company);
                    am_row->SetStringValue(am_protocol, device->Protocol);
                    am_row->SetStringValue(am_protocolType, device->ProtocolType);
                    am_row->SetDecimalValue(am_longitude, device->Longitude);
                    am_row->SetDecimalValue(am_latitude, device->Latitude);
                }

                PhasorDataMapPtr phasorMap;

                // Lookup associated phasor records
                if (TryGetValue(phasorData, deviceName, phasorMap, nullPhasorDataMap))
                {
                    PhasorDataPtr phasor;
                    int32_t sourceIndex = md_row->ValueAsInt32(md_phasorSourceIndex).GetValueOrDefault();

                    if (TryGetValue(*phasorMap, sourceIndex, phasor, nullPhasorData))
                    {
                        am_row->SetInt32Value(am_phasorID, phasor->PhasorID);
                        am_row->SetStringValue(am_phasorType, phasor->PhasorType);
                        am_row->SetStringValue(am_phase, phasor->Phase);
                    }
                }
            }

            activeMeasurements->AddRow(am_row);
        }
    }
}

void DataPublisher::PublishMeasurements(const vector<Measurement>& measurements)
{
    // TODO: Publish measurements to subscribed connections - routing?
}

void DataPublisher::PublishMeasurements(const vector<MeasurementPtr>& measurements)
{
}

const GSF::Guid& DataPublisher::GetNodeID() const
{
    return m_nodeID;
}

void DataPublisher::SetNodeID(const GSF::Guid& nodeID)
{
    m_nodeID = nodeID;
}

SecurityMode DataPublisher::GetSecurityMode() const
{
    return m_securityMode;
}

void DataPublisher::SetSecurityMode(SecurityMode securityMode)
{
    if (IsConnected())
        throw PublisherException("Cannot change security mode once publisher has been connected");

    m_securityMode = securityMode;
}

bool DataPublisher::IsMetadataRefreshAllowed() const
{
    return m_allowMetadataRefresh;
}

void DataPublisher::SetMetadataRefreshAllowed(bool allowed)
{
    m_allowMetadataRefresh = allowed;
}

bool DataPublisher::IsNaNValueFilterAllowed() const
{
    return m_allowNaNValueFilter;
}

void DataPublisher::SetNaNValueFilterAllowed(bool allowed)
{
    m_allowNaNValueFilter = allowed;
}

bool DataPublisher::IsNaNValueFilterForced() const
{
    return m_forceNaNValueFilter;
}

void DataPublisher::SetNaNValueFilterForced(bool forced)
{
    m_forceNaNValueFilter = forced;
}

uint32_t DataPublisher::GetCipherKeyRotationPeriod() const
{
    return m_cipherKeyRotationPeriod;
}

void DataPublisher::SetCipherKeyRotationPeriod(uint32_t period)
{
    m_cipherKeyRotationPeriod = period;
}

void* DataPublisher::GetUserData() const
{
    return m_userData;
}

void DataPublisher::SetUserData(void* userData)
{
    m_userData = userData;
}

uint64_t DataPublisher::GetTotalCommandChannelBytesSent() const
{
    return m_totalCommandChannelBytesSent;
}

uint64_t DataPublisher::GetTotalDataChannelBytesSent() const
{
    return m_totalDataChannelBytesSent;
}

uint64_t DataPublisher::GetTotalMeasurementsSent() const
{
    return m_totalMeasurementsSent;
}

bool DataPublisher::IsConnected() const
{
    return m_connected;
}

void DataPublisher::RegisterStatusMessageCallback(const MessageCallback& statusMessageCallback)
{
    m_statusMessageCallback = statusMessageCallback;
}

void DataPublisher::RegisterErrorMessageCallback(const MessageCallback& errorMessageCallback)
{
    m_errorMessageCallback = errorMessageCallback;
}

void DataPublisher::RegisterClientConnectedCallback(const ClientConnectionCallback& clientConnectedCallback)
{
    m_clientConnectedCallback = clientConnectedCallback;
}

void DataPublisher::RegisterClientDisconnectedCallback(const ClientConnectionCallback& clientDisconnectedCallback)
{
    m_clientDisconnectedCallback = clientDisconnectedCallback;
}