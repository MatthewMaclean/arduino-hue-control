import ConfigParser
import json
import urllib2

AMAZON_URL = "https://pitangui.amazon.com/api/notifications"

Config = ConfigParser.ConfigParser()
Config.read("settings.ini")


def setLights(post_data):
    hue_username = Config.get('Hue', 'Username')
    hue_ip = Config.get('Hue', 'IP')
    hue_url = __make_url(["http://" + hue_ip, "api", hue_username, "lights"])
    __alterLight(hue_url, Config.get('Hue', 'Lights'), post_data)


def __alterLight(hue_url, lights, post_data):
    for light in lights.split(","):
        request = urllib2.Request(__make_url([hue_url, light, "state"]), data=post_data)
        request.get_method = lambda: 'PUT'
        urllib2.urlopen(request)


def getAlarmTime():
    req = urllib2.Request(AMAZON_URL)
    req.add_header('Cookie', "at-main={}; sess-at-main={}; x-main={}; ubid-main={};".format(
        Config.get('Alexa', 'at-main'),
        Config.get('Alexa', 'sess-at-main'),
        Config.get('Alexa', 'x-main'),
        Config.get('Alexa', 'ubid-main')))
    try:
        resp = urllib2.urlopen(req)
    except urllib2.HTTPError:
        return

    alarms = json.load(resp)["notifications"]
    nextAlarm = -1
    for alarm in alarms:
        if alarm["status"] == "ON":
            if nextAlarm == -1 or alarm["alarmTime"] < nextAlarm:
                nextAlarm = alarm["alarmTime"]
    if nextAlarm is not -1:
        print nextAlarm / 1000  # ns -> ms


def __make_url(l):
    return '/'.join(l)
