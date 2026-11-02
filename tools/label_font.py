from PIL import Image, ImageDraw, ImageFont
import numpy as np, sys
F='/home/claude/strongpickle/assets/fonts/'
def render(text, font, cap, track):
    # cap height -> font size
    f = ImageFont.truetype(F+font, 100); bb = f.getbbox('H'); size = 100*cap/(bb[3]-bb[1])
    f = ImageFont.truetype(F+font, int(round(size*4)))   # 4x supersampling
    W=sum(f.getbbox(c)[2]-f.getbbox(c)[0] for c in text)+track*4*(len(text)-1)+40
    im=Image.new('L',(int(W),int(cap*4*2)),0); d=ImageDraw.Draw(im)
    x=20; top=f.getbbox('H')[1]
    for c in text:
        b=f.getbbox(c); d.text((x-b[0], cap*4*0.5-top), c, font=f, fill=255); x+=b[2]-b[0]+track*4
    im=im.resize((im.width//4, im.height//4), Image.LANCZOS)
    a=np.asarray(im); ys,xs=np.where(a>40); return a[ys.min():ys.max()+1, xs.min():xs.max()+1]
if __name__=='__main__':
    for font in ['BarlowCondensed-Medium.ttf','BarlowCondensed-SemiBold.ttf','BarlowCondensed-Bold.ttf']:
        for tr in [5.0,6.0]:
            a=render('SCOOP',font,24,tr); print(font,tr,a.shape)
