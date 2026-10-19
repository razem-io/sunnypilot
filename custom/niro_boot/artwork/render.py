"""Deterministic reveal of the approved artwork. Requires Pillow, numpy, ffmpeg.
Run: python3 custom/niro_boot/artwork/render.py
No generated replacement art, scaling, warping, or device interaction.
"""
from pathlib import Path
import json
import subprocess
import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = Path(__file__).resolve().parent
OUT = ROOT / 'output'
REVIEW = ROOT / 'review'
OUT.mkdir(exist_ok=True)
REVIEW.mkdir(exist_ok=True)
im = Image.open(ROOT / 'source/approved-niro.png').convert('RGB')
W, H = im.size
pixels = np.asarray(im, dtype=np.float32)
y, x = np.mgrid[:H, :W].astype(np.float32)

def smooth(v):
    v = np.clip(v, 0, 1)
    return v * v * (3 - 2 * v)

def mask(poly, blur=5):
    m = Image.new('L', (W, H))
    ImageDraw.Draw(m).polygon(poly, fill=255)
    return np.asarray(m.filter(ImageFilter.GaussianBlur(blur)), dtype=np.float32) / 255

# Geometry follows the approved raster at native resolution. Soft edges sit in
# dark areas, so each feature resolves without visible rectangular transitions.
inside = mask([(250,120),(1420,120),(1550,210),(1605,265),
               (1605,625),(1545,705),(1545,750),(1470,832),
               (201,832),(127,750),(127,705),(65,625),(65,265),(120,210)], 7)
frame = 1 - inside
segments = mask([(505,66),(1180,66),(1180,99),(505,99)], 5)
logo = mask([(505,170),(1160,170),(1160,344),(505,344)], 7)
name = mask([(560,352),(1110,352),(1110,432),(560,432)], 6)
road = smooth((y - 445) / 24) * inside
car = mask([(625,860),(621,706),(630,652),(619,646),(621,631),
            (662,618),(694,578),(715,568),(728,558),(741,570),
            (819,560),(833,548),(847,560),(928,568),(945,556),
            (963,574),(980,608),(1002,617),(1041,628),(1048,650),
            (1032,661),(1051,710),(1050,860)], 5)

branding = np.maximum(logo, name)
rgba = np.dstack((np.asarray(im), np.rint(branding*255).astype(np.uint8)))
brand_image = Image.new("RGBA", (W,H+H%2))
brand_image.paste(Image.fromarray(rgba),(0,0))
brand_image.save(ROOT.parent / "branding.png")
Image.new("RGB",(W,H+H%2)).save(ROOT.parent / "initial.png")

def base_composite(t):
    a = np.full((H,W), float(smooth((t-.12)/1.3)), dtype=np.float32)
    def layer(m, v):
        nonlocal a
        a = a * (1-m) + m*v
    # Symmetric frame trace runs from top center down both sides.
    arrival = .15 + 1.25 * (0.48 * np.abs(x-W/2)/(W/2) + .52*y/H)
    layer(frame, smooth((t-arrival)/.38))
    # Nine individually timed cyan / warm-white segments, left to right.
    index = np.clip(np.floor((x-510)/73),0,8)
    layer(segments, smooth((t-(.42+index*.105))/.22))
    layer(road, smooth((t-(1.10 + .70*(y-460)/(H-460)))/.75))
    layer(car, smooth((t-2.05)/1.1))
    layer(logo, 1.0)
    layer(name, 1.0)
    # Settle to the exact source, including all feather margins.
    a = a + (1-a)*smooth((t-3.85)/.45)
    if t >= 4.3:
        return np.asarray(im)
    return np.rint(pixels*a[...,None]).astype(np.uint8)

# A restrained status line fits between the model name and road horizon.
STATUS = 'Systeme werden hochgefahren'
TEXT_START, TEXT_FADE = 7.8, 1.8
status_layer = Image.new('RGBA', (W*3,H*3))
font = ImageFont.truetype('/System/Library/Fonts/HelveticaNeue.ttc', 23*3)
ImageDraw.Draw(status_layer).text((W*1.5,450*3), STATUS, font=font,
                                 anchor='mm', fill=(169,190,197,255))
status_layer = status_layer.resize((W,H),Image.Resampling.LANCZOS)
status_rgb = np.asarray(status_layer,dtype=np.float32)[...,:3]
status_alpha = np.asarray(status_layer,dtype=np.float32)[...,3:4]/255

def composite(t):
    base = base_composite(t).astype(np.float32)
    opacity = status_alpha*float(smooth((t-TEXT_START)/TEXT_FADE))
    return np.rint(base*(1-opacity)+status_rgb*opacity).astype(np.uint8)

FPS, DURATION = 30, 12
cmd = ['ffmpeg','-y','-loglevel','warning','-f','rawvideo','-pix_fmt','rgb24',
       '-s',f'{W}x{H}','-r',str(FPS),'-i','-','-an','-vf','pad=ceil(iw/2)*2:ceil(ih/2)*2:0:0:black',
       '-c:v','libx264','-preset','slow','-crf','17','-pix_fmt','yuv420p',
       '-movflags','+faststart',str(OUT/'niro-boot-preview.mp4')]
proc = subprocess.Popen(cmd, stdin=subprocess.PIPE)
samples = [0, 1.4, 2.8, 4.3, 7.8, 8.7, 9.6, 11.9]
sample_frames = {round(t*FPS): t for t in samples}
for n in range(FPS*DURATION):
    frame_pixels = composite(n/FPS)
    proc.stdin.write(frame_pixels.tobytes())
    if n <= 130 or 234 <= n <= 287:
        runtime = Image.new("RGB",(W,H+H%2))
        runtime.paste(Image.fromarray(frame_pixels),(0,0))
        runtime.resize((836,471),Image.Resampling.LANCZOS).save(ROOT.parent / "frames" / f"{n:03}.jpg",quality=94)
    assert np.array_equal(frame_pixels[branding == 1], np.asarray(im)[branding == 1]), "Branding moved or faded"
    if n in sample_frames:
        Image.fromarray(frame_pixels).save(REVIEW/f'frame-{sample_frames[n]:.1f}s.png')
proc.stdin.close()
if proc.wait():
    raise RuntimeError('Video encoding failed')
im.save(OUT/'end-frame.png')
assert np.array_equal(base_composite(4.3), np.asarray(im))
assert np.array_equal(composite(7.8), np.asarray(im))
assert np.array_equal(composite(4.3), composite(7.8))
hold = Image.fromarray(composite(10.0))
hold.save(OUT/'final-hold-native.png')
padded = Image.new('RGB',(W,H+H%2))
padded.paste(hold,(0,0))
padded.save(OUT/'final-hold.png')
sheet = Image.new('RGB', (836*2, 500*4), '#14171a')
d = ImageDraw.Draw(sheet)
for i,t in enumerate(samples):
    thumb = Image.open(REVIEW/f'frame-{t:.1f}s.png').resize((836,471))
    xx,yy = (i%2)*836,(i//2)*500
    sheet.paste(thumb,(xx,yy))
    d.text((xx+12, yy+478),f'{t:.1f}s',fill='white')
sheet.save(REVIEW/'contact-sheet.jpg',quality=92)
metadata = {'duration_seconds':DURATION,'fps':FPS,'frames':FPS*DURATION,
            'source_size':[W,H],'video_size':[W+(W%2),H+(H%2)],
            'graphic_complete_seconds':4.3, 'text_start_seconds':TEXT_START,
            'text_fade_seconds':TEXT_FADE, 'final_hold_from_seconds':TEXT_START+TEXT_FADE, 'status_text': STATUS, 'play_once': True,
            'branding_static_from_seconds':0, 'note':'MP4 adds one black bottom row for H.264 4:2:0 compatibility; approved artwork unchanged beneath status text.'}
(OUT/'render-info.json').write_text(json.dumps(metadata,indent=2)+'\n')
print('Rendered 360 frames; delayed text verified; final hold generated; 12.0-second MP4 ready.',flush=True)
