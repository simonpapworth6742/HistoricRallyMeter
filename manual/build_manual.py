#!/usr/bin/env python3
"""Build the Historic Rally Meter user manual (docx) from the screenshots.

Run from anywhere:  python3 manual/build_manual.py
Needs python3-docx (apt install python3-docx).
"""
import os
from docx import Document
from docx.enum.section import WD_ORIENT
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.shared import Inches, Pt, RGBColor

HERE = os.path.dirname(os.path.abspath(__file__))
SHOTS = os.path.join(HERE, "screenshots")
OUT = os.path.join(HERE, "HistoricRallyMeter-UserManual.docx")


def version():
    try:
        with open(os.path.join(HERE, "..", "version.txt")) as f:
            return f.read().strip() or "unknown"
    except OSError:
        return "unknown"


doc = Document()

# Landscape A4 with modest margins: the co-pilot screens are 1280x400, so a
# wide page keeps them readable.
sec = doc.sections[0]
sec.orientation = WD_ORIENT.LANDSCAPE
sec.page_width, sec.page_height = Inches(11.69), Inches(8.27)
for side in ("left_margin", "right_margin"):
    setattr(sec, side, Inches(0.8))
sec.top_margin = sec.bottom_margin = Inches(0.7)

style = doc.styles["Normal"]
style.font.name = "Calibri"
style.font.size = Pt(11)

WIDE = Inches(9.6)     # co-pilot 1280x400 screens
DRIVER = Inches(5.0)   # driver 800x480
PHONE = Inches(2.6)    # phone web client


def h(text, level=1):
    doc.add_heading(text, level=level)


def p(text):
    return doc.add_paragraph(text)


def bullets(items):
    for item in items:
        para = doc.add_paragraph(style="List Bullet")
        if isinstance(item, tuple):
            run = para.add_run(item[0])
            run.bold = True
            para.add_run(" " + item[1])
        else:
            para.add_run(item)


def shot(name, width=WIDE, caption=None):
    path = os.path.join(SHOTS, name)
    if not os.path.exists(path):
        para = p(f"[screenshot {name} missing]")
        para.runs[0].font.color.rgb = RGBColor(0xC0, 0x00, 0x00)
        return
    doc.add_picture(path, width=width)
    doc.paragraphs[-1].alignment = WD_ALIGN_PARAGRAPH.CENTER
    if caption:
        c = p(caption)
        c.alignment = WD_ALIGN_PARAGRAPH.CENTER
        c.runs[0].italic = True
        c.runs[0].font.size = Pt(9)


# ── Cover ────────────────────────────────────────────────────────────────
t = doc.add_paragraph()
t.alignment = WD_ALIGN_PARAGRAPH.CENTER
r = t.add_run("\nHistoric Rally Meter")
r.bold = True
r.font.size = Pt(40)
s = doc.add_paragraph()
s.alignment = WD_ALIGN_PARAGRAPH.CENTER
r = s.add_run("User Manual")
r.font.size = Pt(24)
v = doc.add_paragraph()
v.alignment = WD_ALIGN_PARAGRAPH.CENTER
v.add_run(f"Software version {version()}").font.size = Pt(14)
shot("twinmaster.png", Inches(7.0))
doc.add_page_break()

# ── Introduction ─────────────────────────────────────────────────────────
h("1. What the meter does")
p("The Historic Rally Meter is a distance and average-speed instrument for regularity rallies. "
  "A stage is driven as a series of segments, each at a set average speed over a set distance. "
  "The meter counts wheel or gearbox pulses, converts them to distance with a calibration you "
  "measure yourself, and tells the driver how many seconds ahead or behind the ideal position "
  "the car is at every moment.")
p("There are two displays:")
bullets([
    ("Driver display (800×480)", "a gauge showing seconds ahead or behind, target speed, current speed, "
     "total and trip average speeds. Tones tell the driver to speed up or slow down without looking."),
    ("Co-pilot display (1280×400 touch panel)", "the TwinMaster screen with total, trip and next-segment "
     "distances, and the screens for setting up the stage, calibrating, setting the clock and so on."),
])
p("A phone can also connect over Wi-Fi to see live figures and edit the stage (section 11).")

h("Distances and averages", 2)
bullets([
    ("Total", "distance since the Total was last reset, normally at the stage start."),
    ("Trip", "distance since the Trip was last reset. Reset by the Trip button, by the start of each "
     "segment, and by stage go. Use it to match the road book tulip distances."),
    ("Segment", "distance since the current segment began. Drives the ahead/behind figure."),
    ("Rally clock", "the meter's own clock, adjustable to match the organiser's time without touching the Pi clock."),
])
p("All three distances are measured with the same calibration and can be corrected together on the "
  "Adjust Distance Traveled screen if a wrong turn has been taken (section 4).")

h("Power", 2)
p("The pulse counters stay powered for a few minutes after the Pi is switched off. If the whole meter "
  "has been off longer than that, the counters restart from zero and the meter automatically restarts "
  "Total, Trip and Segment from the moment it is switched on. Settings, segments and the stage memory "
  "are kept in a file on the Pi and survive power loss.")
doc.add_page_break()

# ── Driver display ───────────────────────────────────────────────────────
h("2. Driver display")
shot("driver.png", DRIVER)
bullets([
    ("Gauge and needle", "zero at the top means on time. Needle to the right means slow down, to the left "
     "speed up. The coloured band shows the scale in use: green ±3 s, yellow ±10 s, red ±5 min. The scale "
     "changes as the error grows or shrinks."),
    ("Readout box", "the seconds ahead (+) or behind (−) in large white digits below the needle hub. "
     "In the red scale it reads as hours:minutes:seconds."),
    ("Arrows on the needle", "one, two or three chevrons show the scale (green, yellow, red). During a "
     "segment a short cross-line marks the segment end; the chevrons move towards it as the segment is driven."),
    ("Top left", "current speed over the last two seconds."),
    ("Target", "the target average speed for the current segment."),
    ("Right side", "the average speed since the Total reset (upper) and since the Trip reset (lower)."),
    ("fps / cpu", "display update rate and Pi temperature, for information only."),
    ("Tones", "within a stage, short beeps tell the driver to change speed: C6 when behind, F6 when ahead. "
     "One, two or three beeps per group show how big the change should be. Nothing sounds when within "
     "±0.1 s or more than ±30 s out. The co-pilot can mute them."),
    ("Countdown", "when an auto start is set, T− hh:mm:ss counts down over the gauge and the stage starts "
     "itself at zero."),
])
p("If the Pi has only one screen, a compact version of this gauge appears on the right of the TwinMaster "
  "screen instead of the alarm keypad.")
doc.add_page_break()

# ── TwinMaster ───────────────────────────────────────────────────────────
h("3. TwinMaster screen (co-pilot)")
p("This is the screen the co-pilot uses during the stage. The other screens are reached from its bottom row "
  "and all return here with their back button.")
shot("twinmaster.png")
h("Left panel", 2)
bullets([
    ("Total / Trip buttons", "each shows the distance beside it and the elapsed time underneath. Press the "
     "button to reset that distance and its time. Distances are in metres, switching to km above 999,999 m."),
    ("Adj", "the thin vertical button beside the Total value opens Adjust Distance Traveled (section 4)."),
    ("---> / next / prev", "the third row shows the distance left in the current segment and, to its right, "
     "the target speed of the next segment (END after the last one). Within 500 m of the segment end the "
     "button reads next and, when pressed, ends the segment where the car actually is and starts the next. "
     "Within 500 m after a segment start it reads prev and moves the previous segment's end to here. This "
     "corrects a segment started early or late."),
    ("Trip history", "the last six trip distances, most recent at the top, recorded each time the trip was reset. "
     "Useful to check the tulip distance you have just driven."),
])
h("Right panel", 2)
bullets([
    ("Rally clock", "hh:mm:ss at the top right."),
    ("Mute", "the speaker icon appears while a stage is running and silences the ahead/behind tones. Button beeps, "
     "the alarm and the arrival tone still sound. Stage go always starts unmuted."),
    ("Connect speaker", "shown when a Bluetooth speaker has been remembered on the Setup screen; reconnects it."),
    ("Alarm in km", "press a number to ring alarm.wav when the Total has advanced that many km from now. "
     "The countdown shows below the keypad; clear cancels it. The alarm clears itself five seconds after ringing."),
])
h("Bottom row", 2)
bullets([
    ("stage go / abort stage", "starts the stage (see the dialog below). While a stage is running the button "
     "reads abort stage."),
    ("segments", "Stage Setup (section 5)."),
    ("Adj. driver Zero", "adds or removes seconds from the driver's ahead/behind figure (see the dialog below). "
     "The current adjustment is shown in the label."),
    ("calibration", "Calibration (section 6)."),
    ("date/time", "Date/Time Setup (section 7)."),
    ("Cog", "Setup (section 9)."),
])

h("Stage Go dialog", 2)
shot("dialog_stage_go.png", Inches(7.5))
bullets([
    ("Yes", "resets Total, Trip and Segment, selects the first segment, zeroes the driver adjustment and any "
     "distance adjustment, and starts the tones. Any auto start still pending is cancelled."),
    ("hh:mm button", "the next whole minute at least 10 seconds away. Pressing it stores that minute as the auto "
     "start time, the same as Set on the Auto Start screen. The stage then starts itself at that time."),
    ("Auto start", "opens the Auto Start Setup screen (section 8) to choose a different time."),
    ("No", "closes without changing anything."),
])

h("Abort Stage dialog", 2)
shot("dialog_abort_stage.png", Inches(7.5))
p("Yes stops the stage: the segment is deselected, the tones stop and the driver's target goes blank. "
  "Distances keep counting. No leaves the stage running.")

h("Adjust Driver Zero dialog", 2)
shot("dialog_adj_driver_zero.png", Inches(7.5))
p("Use this when the organiser's timing point shows you are actually on time even though the meter reads "
  "ahead or behind. The dialog shows the correction that would bring the current reading to zero and the "
  "adjustment already in force. Yes applies it, Reset to 0.0 removes any adjustment, No does nothing. "
  "Stage go also resets it to zero.")
doc.add_page_break()

# ── Adjust distance ──────────────────────────────────────────────────────
h("4. Adjust Distance Traveled")
shot("adjust_distance.png")
p("Opened from the Adj button on the Total row. It removes (or adds) distance from everything the meter has "
  "measured since the stage start, so after a wrong turn and the drive back the Total, Trip, segment, "
  "averages and ahead/behind all read as if the detour had not happened.")
bullets([
    ("Readings line", "the current Total, Trip and the offset already applied, in metres."),
    ("Entry", "type with the keypad or use the nudge buttons (−1 km to +1 km), which only change the text. "
     "Nothing changes until apply is pressed."),
    ("+N or −N", "adds to or subtracts from the offset by that many metres. Example: you drove 350 m up a wrong "
     "road and 350 m back, enter −700."),
    ("N (no sign)", "sets the offset so that the Total reads exactly N metres. Use this at a known road book "
     "distance."),
    ("0", "zeroes the offset."),
    ("clear", "zeroes the offset and returns to TwinMaster. Kept well apart from apply."),
    ("apply", "applies the entry, saves and returns to TwinMaster."),
])
p("After a negative adjustment the Trip can read below zero; it counts back up through zero as you drive. "
  "The offset is cleared by a Total reset and by stage go.")
doc.add_page_break()

# ── Stage setup ──────────────────────────────────────────────────────────
h("5. Stage Setup")
shot("stage_setup.png")
p("Enter the segments of the stage here before stage go. Each segment has a target speed in KPH, a distance in "
  "metres and an Auto flag. The Time column shows how long the segment should take.")
bullets([
    ("Editing", "touch a speed or distance to change it with the keypad on the right. Changes apply as you type."),
    ("Auto", "ticked means the meter moves to the next segment itself when this segment's distance is reached. "
     "Unticked means the co-pilot presses next on TwinMaster."),
    ("del", "removes that segment."),
    ("New row", "enter a speed and one or more distances separated by ; to add a segment for each distance at that "
     "speed, then add."),
    ("Set 1–5", "stores the current segment list in that memory slot (confirmation if the slot is in use). "
     "Recall 1–5 loads a slot; a white button means the slot holds a stage. clear memory empties all slots."),
    ("back", "returns to TwinMaster."),
])
h("Overwrite confirmation", 2)
shot("dialog_memory.png", Inches(7.5))
p("Shown when Set is pressed for a slot that already holds a stage. Yes replaces it, No keeps it.")
doc.add_page_break()

# ── Calibration ──────────────────────────────────────────────────────────
h("6. Calibration")
shot("calibration.png")
p("Calibration tells the meter how far the car travels per pulse. It is stored as millimetres per 1000 pulses. "
  "Calibrate over a measured distance, ideally a kilometre or more, marked by the organiser or from a map.")
bullets([
    ("Left column", "live pulse counts from the two sensors, which sensor mode is in use, the current Total, "
     "the pulses it represents, and the calibration in mm/1000p and metres per pulse."),
    ("Start", "at the first marker. Distance and Pulses count up from zero and the rest of the meter keeps working."),
    ("Stop", "at the second marker. The count freezes."),
    ("Actual distance traveled", "correct it to the true distance in metres; the new calibration below updates. "
     "You can also type the calibration directly."),
    ("Set", "saves the new calibration and recalculates every stored segment and memory slot to match. Road book "
     "distances and speeds are unchanged; only the pulse counts behind them move."),
    ("Set sensor 1 / Set both sensors and avg.", "choose one gearbox sensor, or two wheel sensors averaged. "
     "Each asks for confirmation."),
    ("reset to 1m per pulse", "returns to the default 1,000,000 mm/1000p after confirmation."),
])
doc.add_page_break()

# ── Date/Time ────────────────────────────────────────────────────────────
h("7. Date/Time Setup")
shot("datetime.png")
p("The rally clock is the Pi's clock plus an offset, so it can be set to the organiser's time without changing "
  "the Pi.")
bullets([
    ("System Clock / Rally Clock", "both shown live."),
    ("+1h … −1h buttons", "nudge the rally clock by that amount; Zero removes the offset. Each press saves."),
    ("Set Rally Clk", "type a date and time with the keypad (which has / and :) and press Set to make the rally "
     "clock read exactly that. Set returns to TwinMaster."),
    ("NTP time sync", "sets the Pi clock from the internet. Enabled only while the Pi has an internet connection."),
])
doc.add_page_break()

# ── Auto start ───────────────────────────────────────────────────────────
h("8. Auto Start Setup")
shot("auto_start.png")
p("Sets a rally-clock time at which the stage starts itself, exactly as if stage go and Yes had been pressed. "
  "The driver display shows a T− countdown until then. Reached from the Auto start button on the Stage Go dialog.")
bullets([
    ("Rally Clock / Auto Start", "the current time and the start time already set, if any."),
    ("Set Auto Start time within 3 Hours", "enter hh:mm:ss (24 hour). It is prefilled with the next whole minute at "
     "least 30 seconds away. Times more than three hours ahead are refused."),
    ("set", "stores the time. clear removes it. back returns to TwinMaster."),
])
doc.add_page_break()

# ── Setup ────────────────────────────────────────────────────────────────
h("9. Setup")
shot("setup.png")
p("Reached from the cog on TwinMaster. Holds the settings that are rarely changed.")
h("Display section (top)", 2)
bullets([
    ("Title line", "software version and the Pi's hostname."),
    ("Attached displays", "each screen with its size, orientation and gap to the next. The rotate button under a "
     "display turns it 90°; Enable KB puts the on-screen keyboard on that display."),
    ("Reset layout", "puts the 1280×400 co-pilot panel on the left and the 800×480 driver panel to its right. "
     "The meter windows follow after the app is next started."),
    ("force single display mode", "runs the meter as if only one screen existed, with the compact gauge inside "
     "TwinMaster. Takes effect at the next start."),
    ("Web address and QR code", "scan with a phone to open the web client (section 11)."),
])
h("Options (left)", 2)
bullets([
    ("speed units", "KPH or MPH for every displayed speed. Segment target speeds are always entered in KPH."),
    ("Arrival tone 500m before segment end", "when on, a two-note chime sounds once when 500 m remain in the "
     "current segment. A segment shorter than 500 m chimes as it starts. Not affected by mute."),
])
h("Wi-Fi / Bluetooth (right)", 2)
bullets([
    ("Remember bluetooth audio", "records the Bluetooth speaker the Pi is currently using, so the meter can reconnect "
     "it. Connect and Disconnect appear once a speaker is remembered; Connect also reconnects it at every start."),
    ("Hotspot: hostname", "makes the Pi an open Wi-Fi access point named after its hostname, so phones can join it "
     "in the car."),
    ("Join WiFi4hostname", "joins an existing open network named WiFi4 followed by the hostname (a phone hotspot, "
     "for example). The last choice is remembered across reboots."),
    ("Status line", "the current Wi-Fi state and the result of the last action."),
])
h("Bottom row", 2)
bullets([
    ("exit app", "saves and closes the meter."),
    ("Exit & Check for updates", "closes the meter and runs the update script in a terminal. Needs an internet "
     "connection."),
    ("back", "returns to TwinMaster."),
])
doc.add_page_break()

# ── Rally day ────────────────────────────────────────────────────────────
h("10. A typical stage")
for i, step in enumerate([
    "Before the event, calibrate over a measured distance (section 6) and check the rally clock against the "
    "organiser's clock (section 7).",
    "Enter the stage segments from the road book on Stage Setup, or recall a stored stage (section 5).",
    "At the start line press stage go. Press Yes at the flag, or press the hh:mm button to start automatically "
    "on the minute.",
    "Drive to the driver's gauge and tones. The co-pilot follows the road book with Trip and the Trip history, "
    "and presses next or prev if a segment change was early or late.",
    "After a wrong turn, use Adj to remove the detour distance (section 4). If a timing point shows the meter "
    "is out, use Adj. driver Zero.",
    "After the last segment the target shows END and the tones stop. Press abort stage to clear the stage, "
    "or stage go for the next one.",
], start=1):
    doc.add_paragraph(step, style="List Number")
doc.add_page_break()

# ── Web client ───────────────────────────────────────────────────────────
h("11. Phone web client")
p("Any phone on the same Wi-Fi (the Pi hotspot, or a network both have joined) can open the address shown on the "
  "Setup screen, or scan its QR code. The page has two tabs.")
tbl = doc.add_table(rows=1, cols=2)
c0, c1 = tbl.rows[0].cells
c0.paragraphs[0].add_run().add_picture(os.path.join(SHOTS, "web_live.png"), width=PHONE)
c1.paragraphs[0].add_run().add_picture(os.path.join(SHOTS, "web_setup.png"), width=PHONE)
c0.add_paragraph("Live").alignment = WD_ALIGN_PARAGRAPH.CENTER
c1.add_paragraph("Setup").alignment = WD_ALIGN_PARAGRAPH.CENTER
bullets([
    ("Live", "rally clock, seconds ahead/behind, current and target speed, Trip and Total with their averages, "
     "the segment number, the same next/prev button as TwinMaster, and Reset Trip."),
    ("Setup", "the segment list, editable in place, Add segment, the five memory slots with Store and Recall, "
     "and Reset Trip / Reset Total."),
])
p("Everything a phone does goes through the same functions as the touch screen, and every connected phone and the "
  "panel are updated together.")

doc.save(OUT)
print("wrote", OUT)
