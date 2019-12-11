#include "SMC100.h"

const char SMC100::CarriageReturnCharacter = '\r';
const char SMC100::NewLineCharacter = '\n';
const char SMC100::GetCharacter = '?';
const char SMC100::NoErrorCharacter = '@';
const uint32_t SMC100::WipeInputEvery = 100000;
const uint32_t SMC100::CommandReplyTimeMax = 500000;
const uint32_t SMC100::WaitAfterSendingTimeMax = 20000;

const SMC100::CommandStruct SMC100::CommandLibrary[] =
{
	{CommandType::None,"  ",CommandParameterType::None,CommandGetSetType::None},
	{CommandType::Enable,"MM",CommandParameterType::Int,CommandGetSetType::GetSet},
	{CommandType::Home,"OR",CommandParameterType::None,CommandGetSetType::None},
	{CommandType::MoveAbs,"PA",CommandParameterType::Float,CommandGetSetType::GetSet},
	{CommandType::MoveRel,"PR",CommandParameterType::Float,CommandGetSetType::GetSet},
	{CommandType::MoveEstimate,"PT",CommandParameterType::Float,CommandGetSetType::GetAlways},
	{CommandType::Configure,"PW",CommandParameterType::Int,CommandGetSetType::GetSet},
	{CommandType::Analogue,"RA",CommandParameterType::None,CommandGetSetType::GetAlways},
	{CommandType::GPIOInput,"RB",CommandParameterType::None,CommandGetSetType::GetAlways},
	{CommandType::Reset,"RS",CommandParameterType::None,CommandGetSetType::None},
	{CommandType::GPIOOutput,"SB",CommandParameterType::None,CommandGetSetType::GetSet},
	{CommandType::LimitPositive,"SR",CommandParameterType::Float,CommandGetSetType::GetSet},
	{CommandType::LimitNegative,"SL",CommandParameterType::Float,CommandGetSetType::GetSet},
	{CommandType::PositionAsSet,"TH",CommandParameterType::None,CommandGetSetType::GetAlways},
	{CommandType::PositionReal,"TP",CommandParameterType::None,CommandGetSetType::GetAlways},
	{CommandType::KeypadEnable,"JM",CommandParameterType::Int,CommandGetSetType::GetSet},
	{CommandType::ErrorCommands,"TE",CommandParameterType::None,CommandGetSetType::GetAlways},
	{CommandType::ErrorHardware,"TS",CommandParameterType::None,CommandGetSetType::GetAlways}
};

const SMC100::StatusCharSet SMC100::StatusLibrary[] =
{
	{"0A",StatusType::NoReference},
	{"0B",StatusType::NoReference},
	{"0C",StatusType::NoReference},
	{"0D",StatusType::NoReference},
	{"0E",StatusType::NoReference},
	{"0F",StatusType::NoReference},
	{"10",StatusType::NoReference},
	{"11",StatusType::NoReference},
	{"14",StatusType::NoReference},
	{"1E",StatusType::Homing},
	{"1F",StatusType::Homing},
	{"28",StatusType::Moving},
	{"32",StatusType::Ready},
	{"33",StatusType::Ready},
	{"34",StatusType::Ready},
	{"35",StatusType::Ready},
	{"3C",StatusType::Disabled},
	{"3D",StatusType::Disabled},
	{"3E",StatusType::Disabled},
	{"46",StatusType::Jogging},
	{"47",StatusType::Jogging},
};

SMC100::SMC100(HardwareSerial *serial, uint8_t address)
{
	SerialPort = serial;
	Address = address;
	CurrentCommand = NULL;
	CurrentCommandParameter = 0.0;
	ReplyBufferIndex = 0;
	for (uint8_t Index = 0; Index < SMC100ReplyBufferSize; ++Index)
	{
		ReplyBuffer[Index] = 0;
	}
	ClearCommandQueue();
	Mode = ModeType::Inactive;
	Busy = false;
	HasBeenHomed = false;
	AllCompleteCallback = NULL;
	MoveCompleteCallback = NULL;
	HomeCompleteCallback = NULL;
	GPIOReturnCallback = NULL;
	NeedToFireMoveComplete = false;
	NeedToFireHomeComplete = false;
	CurrentCommand = NULL;
	GPIOInput = 0;
	GPIOOutput = 0;
	Position = 0.0;
	PositionLimitNegative = 0.0;
	PositionLimitPositive = 0.0;
	LastWipeTime = 0;
	TransmitTime = 0;
	Mode = ModeType::Inactive;
	Status = StatusType::Unknown;
}

void SMC100::Begin()
{
	CommandQueuePut(CommandType::ErrorHardware, 0.0, CommandGetSetType::None);
	CommandQueuePut(CommandType::LimitPositive, 0.0, CommandGetSetType::Get);
	CommandQueuePut(CommandType::LimitNegative, 0.0, CommandGetSetType::Get);
	CommandQueuePut(CommandType::GPIOInput, 0.0, CommandGetSetType::None);
	Mode = ModeType::Idle;
}

bool SMC100::IsHomed()
{
	return HasBeenHomed;
}

bool SMC100::IsReady()
{
	if (Status == StatusType::Ready)
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool SMC100::IsMoving()
{
	if (Status == StatusType::Moving)
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool SMC100::IsEnabled()
{
	if (Status != StatusType::Disabled)
	{
		return true;
	}
	else
	{
		return false;
	}
}

void SMC100::Enable(bool Setting)
{
	float ParamterValue = 0.0;
	if (Setting)
	{
		ParamterValue = 1.0;
	}
	CommandQueuePut(CommandType::Enable, ParamterValue, CommandGetSetType::Set);
}

bool SMC100::IsBusy()
{
	return Busy;
}

void SMC100::Home()
{
	CommandQueuePut(CommandType::Home, 0.0, CommandGetSetType::None);
}

void SMC100::MoveAbsolute(float Target)
{
	if (Target < PositionLimitNegative)
	{
		Target = PositionLimitNegative;
	}
	if (Target > PositionLimitPositive)
	{
		Target = PositionLimitPositive;
	}
	CommandQueuePut(CommandType::MoveAbs, Target, CommandGetSetType::Set);
}

float SMC100::GetPosition()
{
	return Position;
}

void SMC100::SendGetGPIOInput()
{
	CommandQueuePut(CommandType::GPIOInput, 0.0, CommandGetSetType::None);
}

bool SMC100::GetGPIOInput(uint8_t Pin)
{
	if (Pin > 3)
	{
		Pin = 3;
	}
	return bitRead(GPIOInput, Pin);
}

void SMC100::SetGPIOOutput(uint8_t Pin, bool Output)
{
	if (Pin > 3)
	{
		Pin = 3;
	}
	bitWrite(GPIOOutput, Pin, Output);
	CommandQueuePut(CommandType::GPIOOutput, (float)GPIOOutput, CommandGetSetType::Set);
}

void SMC100::SetGPIOOutputAll(uint8_t Code)
{
	GPIOOutput = Code;
	CommandQueuePut(CommandType::GPIOOutput, (float)GPIOOutput, CommandGetSetType::Set);
}

void SMC100::SetAllCompleteCallback(FinishedListener Callback)
{
	AllCompleteCallback = Callback;
}

void SMC100::SetHomeCompleteCallback(FinishedListener Callback)
{
	HomeCompleteCallback = Callback;
}

void SMC100::SetMoveCompleteCallback(FinishedListener Callback)
{
	MoveCompleteCallback = Callback;
}

void SMC100::SetGPIOReturnCallback(FinishedListener Callback)
{
	GPIOReturnCallback = Callback;
}

void SMC100::Check()
{
	switch (Mode)
	{
		case ModeType::Idle:
			CheckCommandQueue();
			break;
		case ModeType::WaitAfterSendingCommand:
			CheckWaitAfterSending();
			break;
		case ModeType::WaitForCommandReply:
			CheckForCommandReply();
			break;
		default:
			break;
	}
}

void SMC100::CheckCommandQueue()
{
	bool NewCommandPulled = CommandQueuePullToCurrentCommand();
	if (NewCommandPulled)
	{
		if (CurrentCommand != NULL)
		{
			Busy = true;
			SendCurrentCommand();
		}
		else
		{
			Serial.print("<SMC100>(Command in queue is null.)\n");
		}
	}
	else
	{
		if (Busy)
		{
			Busy = false;
			if (AllCompleteCallback != NULL)
			{
				AllCompleteCallback();
			}
		}
		if ( (micros() - LastWipeTime) > WipeInputEvery )
		{
			LastWipeTime = micros();
			if (SerialPort->available())
			{
				//uint8_t ByteRead = SerialPort->read();
				//Serial.print("J:");
				//Serial.print(ByteRead);
				//Serial.print("\n");
				SerialPort->read();
			}
		}
	}
}

void SMC100::CheckWaitAfterSending()
{
	if ( (micros() - TransmitTime) > WaitAfterSendingTimeMax )
	{
		SendErrorCommandRequest();
	}
}

void SMC100::CheckForCommandReply()
{
	if (SerialPort->available())
	{
		char NewChar = SerialPort->read();
		if (NewChar == CarriageReturnCharacter)
		{

		}
		else if (NewChar == NewLineCharacter)
		{
			ReplyBuffer[ReplyBufferIndex] = '\0';
			ParseReply();
		}
		else
		{
			ReplyBuffer[ReplyBufferIndex] = NewChar;
			ReplyBufferIndex++;
			if (ReplyBufferIndex > SMC100ReplyBufferSize)
			{
				ReplyBuffer[SMC100ReplyBufferSize-1] = '\0';
				Serial.print("<SMC100>(Error: Buffer overflow with ");
				Serial.print(ReplyBuffer);
				Serial.print(")\n");
				Mode = ModeType::Idle;
			}
		}
	}
	if ( (micros() - TransmitTime) > CommandReplyTimeMax )
	{
		Mode = ModeType::Idle;
		Serial.print("<SMC200>(Time out detected.)\n");
	}
}

void SMC100::ParseReply()
{
	char* EndOfAddress;
	char* ParameterAddress;
	uint8_t AddressOfReply = strtol(ReplyBuffer, &EndOfAddress, 10);
	if (AddressOfReply != Address)
	{
		Serial.print("<SMC100>(Address does not match return for ");
		Serial.print(ReplyBuffer);
		Serial.print(")\n");
	}
	else if ( (CurrentCommand->CommandChar[0] != *EndOfAddress) || (CurrentCommand->CommandChar[1] != *(EndOfAddress + 1)) )
	{
		Serial.print("<SMC100>(Return string expected ");
		Serial.print(CurrentCommand->CommandChar[0]);
		Serial.print(CurrentCommand->CommandChar[1]);
		Serial.print(" but received ");
		Serial.print(*EndOfAddress);
		Serial.print(*(EndOfAddress + 1));
		Serial.print(")\n");
	}
	else
	{
		ParameterAddress = EndOfAddress + 2;
		//char* EndOfReplyData = ReplyData + ReplyBufferIndex;
		if (CurrentCommand->Command == CommandType::PositionReal)
		{
			Position = atof(ParameterAddress);
			if (NeedToFireMoveComplete)
			{
				NeedToFireMoveComplete = false;
				if (MoveCompleteCallback != NULL)
				{
					MoveCompleteCallback();
				}
			}
			if (NeedToFireHomeComplete)
			{
				NeedToFireHomeComplete = false;
				if (HomeCompleteCallback != NULL)
				{
					HomeCompleteCallback();
				}
			}
			Mode = ModeType::Idle;
		}
		else if (CurrentCommand->Command == CommandType::ErrorCommands)
		{
			if (*ParameterAddress != NoErrorCharacter)
			{
				Serial.print("<SMC100>(Error code: ");
				Serial.print(*ParameterAddress);
				Serial.print(")\n");
				Mode = ModeType::Idle;
			}
			else
			{
				SendErrorHardwareRequest();
			}
		}
		else if (CurrentCommand->Command == CommandType::ErrorHardware)
		{
			bool ErrorStatus = false;
			char ErrorCode[4];
			for (uint8_t Index = 0; Index < 4; Index++)
			{
				ErrorCode[Index] = *(ParameterAddress + Index);
				if (ErrorCode[Index] != '0')
				{
					ErrorStatus = true;
				}
			}
			if (ErrorStatus)
			{
				Serial.print("<SMC100>(Error hardware code: ");
				Serial.print(ErrorCode);
				Serial.print(")\n");
				Mode = ModeType::Idle;
			}
			char StatusChar[3];
			StatusChar[0] = *(ParameterAddress + 4);
			StatusChar[1] = *(ParameterAddress + 5);
			StatusChar[2] = '\0';
			Status = ConvertStatus(StatusChar);
			if (Status == StatusType::Error)
			{
				Serial.print("<SMC100>(Error status code not recognized: ");
				Serial.print(StatusChar);
				Serial.print(")\n");
				Mode = ModeType::Idle;
			}
			else if ( Status == StatusType::NoReference )
			{
				HasBeenHomed = false;
				Mode = ModeType::Idle;
			}
			else if ( Status == StatusType::Homing )
			{
				HasBeenHomed = false;
				SendErrorHardwareRequest();
			}
			else if ( Status == StatusType::Moving )
			{
				HasBeenHomed = true;
				SendErrorHardwareRequest();
			}
			else if ( Status == StatusType::Ready )
			{
				HasBeenHomed = true;
				SendPositionRequest();
			}
		}
		else if (CurrentCommand->Command == CommandType::GPIOInput)
		{
			GPIOInput = (uint8_t)atoi(ParameterAddress);
			SendErrorCommandRequest();
			if (GPIOReturnCallback != NULL)
			{
				GPIOReturnCallback();
			}
		}
		else if (CurrentCommand->Command == CommandType::Analogue)
		{
			AnalogueReading = atof(ParameterAddress);
			SendErrorCommandRequest();
		}
		else if ( (CurrentCommand->Command == CommandType::LimitNegative) )
		{
			if (CurrentCommandGetOrSet == CommandGetSetType::Get)
			{
				PositionLimitNegative = atof(ParameterAddress);
				SendErrorCommandRequest();
			}
			else
			{
				SendGetLimitNegative();
			}
		}
		else if (CurrentCommand->Command == CommandType::LimitPositive)
		{
			if (CurrentCommandGetOrSet == CommandGetSetType::Get)
			{
				PositionLimitPositive = atof(ParameterAddress);
				SendErrorCommandRequest();
			}
			else
			{
				SendGetLimitPositive();
			}
		}
		else
		{
			SendErrorCommandRequest();
		}
	}
}

SMC100::StatusType SMC100::ConvertStatus(char* StatusChar)
{
	for (int Index = 0; Index < 21; ++Index)
	{
		if ( strcmp( StatusChar, StatusLibrary[Index].Code ) == 0)
		{
			return StatusLibrary[Index].Type;
		}
	}
	return StatusType::Error;
}

bool SMC100::SendCurrentCommand()
{
	bool Status = true;
	if (CurrentCommand->Command == CommandType::None)
	{
		Mode = ModeType::Idle;
		Serial.print("<SMC100>(Empty command requested.)");
		return false;
	}
	//Serial.print(Address);
	//Serial.print(CurrentCommand->CommandChar[0]);
	//Serial.print(CurrentCommand->CommandChar[1]);
	SerialPort->print(Address);
	SerialPort->write(CurrentCommand->CommandChar[0]);
	SerialPort->write(CurrentCommand->CommandChar[1]);
	if (CurrentCommandGetOrSet == CommandGetSetType::Get)
	{
		Serial.write(GetCharacter);
		SerialPort->write(GetCharacter);
	}
	else if (CurrentCommandGetOrSet == CommandGetSetType::Set)
	{
		if (CurrentCommand->SendType == CommandParameterType::Int)
		{
			//Serial.print((int)(CurrentCommandParameter));
			SerialPort->print((int)(CurrentCommandParameter));
		}
		else if (CurrentCommand->SendType == CommandParameterType::Float)
		{
			//Serial.print(CurrentCommandParameter,6);
			SerialPort->print(CurrentCommandParameter,6);
		}
		else
		{
			Status = false;
		}
	}
	else if ( (CurrentCommand->GetSetType == CommandGetSetType::None) || (CurrentCommand->GetSetType == CommandGetSetType::GetAlways) )
	{

	}
	else
	{
		Status = false;
	}
	//Serial.print(NewLineCharacter);
	SerialPort->write(CarriageReturnCharacter);
	SerialPort->write(NewLineCharacter);
	ReplyBufferIndex = 0;
	TransmitTime = micros();
	if ( (CurrentCommand->Command == CommandType::MoveAbs) || (CurrentCommand->Command == CommandType::MoveRel) )
	{
		if (CurrentCommandGetOrSet == CommandGetSetType::Set)
		{
			NeedToFireMoveComplete = true;
		}
	}
	if ( (CurrentCommand->Command == CommandType::Home) )
	{
		if (CurrentCommandGetOrSet == CommandGetSetType::Set)
		{
			NeedToFireHomeComplete = true;
		}
	}
	if ( (CurrentCommandGetOrSet == CommandGetSetType::Get) || (CurrentCommand->GetSetType == CommandGetSetType::GetAlways) )
	{
		Mode = ModeType::WaitForCommandReply;
	}
	else
	{
		Mode = ModeType::WaitAfterSendingCommand;
	}
	return Status;
}

void SMC100::ClearCommandQueue()
{
	for (int Index = 0; Index < SMC100QueueCount; ++Index)
	{
		CommandQueue[Index].Command = NULL;
		CommandQueue[Index].Parameter = 0.0;
		CommandQueue[Index].GetOrSet = CommandGetSetType::None;
	}
	CommandQueueHead = 0;
	CommandQueueTail = 0;
	CommandQueueFullFlag = false;
}
bool SMC100::CommandQueueFull()
{
	return CommandQueueFullFlag;
}
bool SMC100::CommandQueueEmpty()
{
	return ( !CommandQueueFullFlag && (CommandQueueHead == CommandQueueTail) );
}
uint8_t SMC100::CommandQueueCount()
{
	uint8_t Count = SMC100QueueCount;
	if(!CommandQueueFullFlag)
	{
		if(CommandQueueHead >= CommandQueueTail)
		{
			Count = (CommandQueueHead - CommandQueueTail);
		}
		else
		{
			Count = (SMC100QueueCount + CommandQueueHead - CommandQueueTail);
		}
	}
	return Count;
}
void SMC100::CommandQueueAdvance()
{
	if(CommandQueueFullFlag)
	{
		CommandQueueTail = (CommandQueueTail + 1) % SMC100QueueCount;
	}
	CommandQueueHead = (CommandQueueHead + 1) % SMC100QueueCount;
	CommandQueueFullFlag = (CommandQueueHead == CommandQueueTail);
}
void SMC100::CommandQueueRetreat()
{
	CommandQueueFullFlag = false;
	CommandQueueTail = (CommandQueueTail + 1) % SMC100QueueCount;
}
void SMC100::SendGetLimitNegative()
{
	CommandCurrentPut(CommandType::LimitNegative, 0.0, CommandGetSetType::Get);
	SendCurrentCommand();
}
void SMC100::SendGetLimitPositive()
{
	CommandCurrentPut(CommandType::LimitPositive, 0.0, CommandGetSetType::Get);
	SendCurrentCommand();
}
void SMC100::SendErrorCommandRequest()
{
	CommandCurrentPut(CommandType::ErrorCommands, 0.0, CommandGetSetType::None);
	SendCurrentCommand();
}
void SMC100::SendErrorHardwareRequest()
{
	CommandCurrentPut(CommandType::ErrorHardware, 0.0, CommandGetSetType::None);
	SendCurrentCommand();
}
void SMC100::SendPositionRequest()
{
	CommandCurrentPut(CommandType::PositionReal, 0.0, CommandGetSetType::None);
	SendCurrentCommand();
}
void SMC100::CommandCurrentPut(CommandType Type, float Parameter, CommandGetSetType GetOrSet)
{
	//CurrentCommand = const_cast<CommandStruct*>(&CommandLibrary[static_cast<uint8_t>(Type)]);
	CurrentCommand = &CommandLibrary[static_cast<uint8_t>(Type)];
	CurrentCommandParameter = Parameter;
	CurrentCommandGetOrSet = GetOrSet;
}
void SMC100::CommandQueuePut(CommandType Type, float Parameter, CommandGetSetType GetOrSet)
{
	//CommandStruct* CommandPointer = const_cast<CommandStruct*>(&CommandLibrary[static_cast<uint8_t>(Type)]);
	const CommandStruct* CommandPointer = &CommandLibrary[static_cast<uint8_t>(Type)];
	CommandQueuePut(CommandPointer, Parameter, GetOrSet);
}
void SMC100::CommandQueuePut(const CommandStruct* CommandPointer, float Parameter, CommandGetSetType GetOrSet)
{
	CommandQueue[CommandQueueHead].Command = CommandPointer;
	CommandQueue[CommandQueueHead].Parameter = Parameter;
	CommandQueue[CommandQueueHead].GetOrSet = GetOrSet;
	CommandQueueAdvance();
}
bool SMC100::CommandQueuePullToCurrentCommand()
{
	bool Status = false;
	if (!CommandQueueEmpty())
	{
		CurrentCommand = CommandQueue[CommandQueueTail].Command;
		CurrentCommandParameter = CommandQueue[CommandQueueTail].Parameter;
		CurrentCommandGetOrSet = CommandQueue[CommandQueueTail].GetOrSet;
		CommandQueueRetreat();
		Status = true;
		//Serial.print("NP");
		//Serial.print(CurrentCommandParameter);
		//Serial.print("\n");
	}
	return Status;
}
