#include "Actuation.hpp"
#include "Configuration.hpp"

#include <sstream>
#include <cstring>

Actuation::Actuation()
{
	_repeatRuleSequence = false;
	_entryRuleId = 0;
	_deviceId = "UnknownDevice";
}

Actuation::~Actuation()
{
}

void Actuation::setDeviceId(const std::string& deviceId)
{
	_deviceId = deviceId;
}

std::string Actuation::getDeviceId() const
{
	return _deviceId;
}

bool Actuation::loadRuleDefinition(Configuration& ruleConfiguration)
{
	clearRules();

	if (ruleConfiguration.isSection("General"))
	{
		_repeatRuleSequence = ruleConfiguration.getValue("General", "repeat_rule_sequence", "0") == "1";
	}

	std::list<std::string> sections = ruleConfiguration.getSections();
	for (std::list<std::string>::const_iterator it = sections.begin(); it != sections.end(); ++it)
	{
		if (it->compare(0, 5, "RULE_") != 0)
		{
			continue;
		}

		Rule rule;
		std::memset(&rule, 0, sizeof(rule));

		std::string ruleIdText = it->substr(5);
		rule.id = parseUnsignedLong(ruleIdText);
		rule.condition = parseConditionType(ruleConfiguration.getValue(*it, "condition", "=="));
		rule.sensorValue = parseLong(ruleConfiguration.getValue(*it, "sensor_value", "0"));
		rule.actionType = parseActionType(ruleConfiguration.getValue(*it, "action_type", "gpio"));
		rule.actionIo = parseUnsignedLong(ruleConfiguration.getValue(*it, "action_io", "0"));
		rule.actionValue = parseLong(ruleConfiguration.getValue(*it, "action_value", "0"));
		rule.ruleChained = ruleConfiguration.getValue(*it, "rule_chained", "false") == "true";
		rule.nextRuleTrue = parseUnsignedLong(ruleConfiguration.getValue(*it, "next_rule_true", "0"));
		rule.nextRuleFalse = parseUnsignedLong(ruleConfiguration.getValue(*it, "next_rule_false", "0"));

		_rules.push_back(rule);

		if (_entryRuleId == 0 || rule.id < _entryRuleId)
		{
			_entryRuleId = rule.id;
		}
	}

	return !_rules.empty();
}

bool Actuation::evaluate(unsigned long sensorValue, ActuationCallback* callback)
{
	if (_rules.empty() || callback == nullptr)
	{
		return false;
	}

	unsigned long currentRuleId = _entryRuleId;
	if (currentRuleId == 0)
	{
		return false;
	}

	bool anyRuleMatched = false;
	unsigned long safetyCounter = 0;
	unsigned long safetyLimit = static_cast<unsigned long>(_rules.size()) * 3;
	if (safetyLimit < 8)
	{
		safetyLimit = 8;
	}

	while (currentRuleId != 0 && safetyCounter < safetyLimit)
	{
		Rule* rule = findRuleById(currentRuleId);
		if (rule == nullptr)
		{
			break;
		}

		bool isMatch = false;
		if (rule->condition == ConditionClock)
		{
			isMatch = evaluateClockCondition(*rule);
		}
		else
		{
			isMatch = evaluateCondition(static_cast<long>(sensorValue), rule->condition, rule->sensorValue);
		}
		if (isMatch)
		{
			ActuationDecision decision;
			decision.ruleId = rule->id;
			decision.conditionMatched = true;
			decision.actionType = rule->actionType;
			decision.actionIo = rule->actionIo;
			decision.actionValue = rule->actionValue;

			callback->onActionTriggered(decision);
			anyRuleMatched = true;
		}

		if (!rule->ruleChained)
		{
			break;
		}

		if (isMatch)
		{
			currentRuleId = rule->nextRuleTrue;
		}
		else
		{
			currentRuleId = rule->nextRuleFalse;
		}

		if (currentRuleId == 0 && _repeatRuleSequence)
		{
			currentRuleId = _entryRuleId;
		}

		safetyCounter++;
	}

	callback->onRuleEvaluationComplete(sensorValue);
	return anyRuleMatched;
}

void Actuation::clearRules()
{
	_rules.clear();
	_entryRuleId = 0;
	_lastClockTrigger.clear();
}

std::list<Rule> Actuation::getRules()
{
	return _rules;
}

RuleConditionType Actuation::parseConditionType(const std::string& value)
{
	if (value == ">")
	{
		return ConditionGreaterThan;
	}
	if (value == "<")
	{
		return ConditionLessThan;
	}
	if (value == "==")
	{
		return ConditionEqualTo;
	}
	if (value == ">=")
	{
		return ConditionGreaterOrEqual;
	}
	if (value == "<=")
	{
		return ConditionLessOrEqual;
	}
	if (value == "!=")
	{
		return ConditionNotEqualTo;
	}
	if (value == "clock" || value == "timer")
	{
		return ConditionClock;
	}

	return ConditionUnknown;
}

ActuatorActionType Actuation::parseActionType(const std::string& value)
{
	if (value == "gpio")
	{
		return ActionGpio;
	}
	if (value == "pwm")
	{
		return ActionPwm;
	}

	return ActionUnknown;
}

bool Actuation::evaluateCondition(long sensorValue, RuleConditionType condition, long ruleValue)
{
	switch (condition)
	{
		case ConditionGreaterThan:
		{
			return sensorValue > ruleValue;
		}
		case ConditionLessThan:
		{
			return sensorValue < ruleValue;
		}
		case ConditionEqualTo:
		{
			return sensorValue == ruleValue;
		}
		case ConditionGreaterOrEqual:
		{
			return sensorValue >= ruleValue;
		}
		case ConditionLessOrEqual:
		{
			return sensorValue <= ruleValue;
		}
		case ConditionNotEqualTo:
		{
			return sensorValue != ruleValue;
		}
		case ConditionClock:
		{
			return false;
		}
		case ConditionUnknown:
		default:
		{
			return false;
		}
	}
}

bool Actuation::evaluateClockCondition(const Rule& rule)
{
	if (rule.sensorValue <= 0)
	{
		return false;
	}

	const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
	std::map<unsigned long, std::chrono::steady_clock::time_point>::iterator it = _lastClockTrigger.find(rule.id);
	if (it == _lastClockTrigger.end())
	{
		_lastClockTrigger[rule.id] = now;
		return false;
	}

	const unsigned long intervalSeconds = static_cast<unsigned long>(rule.sensorValue);
	const std::chrono::steady_clock::duration elapsed = now - it->second;
	if (elapsed >= std::chrono::seconds(intervalSeconds))
	{
		it->second = now;
		return true;
	}

	return false;
}

unsigned long Actuation::parseUnsignedLong(const std::string& value)
{
	std::stringstream stream(value);
	unsigned long result = 0;
	stream >> result;
	return result;
}

long Actuation::parseLong(const std::string& value)
{
	std::stringstream stream(value);
	long result = 0;
	stream >> result;
	return result;
}

Rule* Actuation::findRuleById(unsigned long ruleId)
{
	for (std::list<Rule>::iterator it = _rules.begin(); it != _rules.end(); ++it)
	{
		if (it->id == ruleId)
		{
			return &(*it);
		}
	}

	return nullptr;
}
