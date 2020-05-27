import cgi
import requests
SAVE_DIR = 'D:/test/GOGH Vincent van'
index = 1

def downloadURL(url):
	global index
	r = requests.get(url.rstrip(), stream = True)
	if r.status_code == 200:
		targetFileName = "{0:01}.jpg".format(index)
		with open("{}/{}".format(SAVE_DIR, targetFileName), 'wb') as f:
			for chunk in r.iter_content(chunk_size = 1024):
				f.write(chunk)
			index += 1
	return targetFileName
with open('D:/test/1.csv') as f:
	print(list(map(downloadURL, f.readlines())))