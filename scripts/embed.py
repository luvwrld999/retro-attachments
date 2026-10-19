from pathlib import Path
out=Path('build'); out.mkdir(exist_ok=True)
with (out/'assets.hpp').open('w') as f:
    for name,path in [('font_data','assets/Lato-Regular.ttf'),('ca_data','assets/cacert.pem')]:
        data=Path(path).read_bytes()
        f.write('static const unsigned char '+name+'[] = {\n')
        for i in range(0,len(data),24): f.write(','.join(str(x) for x in data[i:i+24])+',\n')
        f.write('0};\n')
