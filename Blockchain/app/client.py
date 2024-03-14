#!/bin/python3
import json
import os
import requests
import socket
from user import User
from flask import Flask, render_template, redirect, request,send_file,session
from werkzeug.utils import secure_filename
from app import app
from timeit import default_timer as timer

ADDR = "http://127.0.0.1:80"
#ADDR = "https://" + socket.gethostbyname(socket.gethostname()) + ":80"
print("HOST=", ADDR)
# Stores all the post transaction in the node
request_tx = []
pending_files = []
#store filename
files = {}
admin = User()
#destiantion for upload files
UPLOAD_FOLDER = "app/Uploads"
app.config['UPLOAD_FOLDER'] = UPLOAD_FOLDER

#peers list
peers = []

app = Flask(__name__)
app.secret_key = '7avQ3nV'  # Set a secret key for security purposes

def get_tx_req(): #get blockchain
    global request_tx
    chain_addr = "{0}/chain".format(ADDR)
    resp = requests.get(chain_addr)
    if resp.status_code == 200:
        content = []
        chain = json.loads(resp.content.decode())
        for block in chain["chain"]:
            block["user"] = block["transactions"]["user"]
            block["description"] = block["transactions"]["description"]
            block["v_file"] = block["transactions"]["v_file"]
            block["file_data"] = block["transactions"]["file_data"]
            block["file_size"] = block["transactions"]["file_size"]
            #bytecode file test
            with open(block["transactions"]["v_file"], 'wb') as file:
                file.write(block["transactions"]["file_data"].encode())
            # Read the file back and reconstruct it
            with open(block["transactions"]["v_file"], 'rb') as file:
                print("FILE==",file.read())
            content.append(block)
        request_tx = content#sorted(content,key=lambda k: k["hash"],reverse=True)


def get_pending(): #get blockchain
    global pending_files
    chain_addr = "{0}/pending".format(ADDR)
    resp = requests.get(chain_addr)
    if resp.status_code == 200:
        content = []
        chain = json.loads(resp.content.decode())
        for block in chain["chain"]:
            block["user"] = block["transactions"]["user"]
            block["description"] = block["transactions"]["description"]
            block["v_file"] = block["transactions"]["v_file"]
            block["file_data"] = block["transactions"]["file_data"]
            block["file_size"] = block["transactions"]["file_size"]
            content.append(block)
        pending_files = content#sorted(content,key=lambda k: k["hash"],reverse=True)
        # Write the bytecode to a file

def getValidity():
    address="{0}/valid".format(ADDR)
    resp = requests.get(address)
    if resp.status_code == 200:
        code = resp.content.decode()
        print("RECEIVED CODE = ", code)
        if(code=="Chain In Order"):
            print("Returning True")
            return True
        else:
            print("Returning False")
            return False


@app.route("/")
def index():
    if not session.get('logged_in'):
        return redirect("/login")  # Redirect to login if not logged in
    get_pending()
    get_tx_req()
    return render_template("index.html",title="FileStorage",subtitle = "A Decentralized Network for File Storage/Sharing",node_address = ADDR,request_tx = request_tx, pending_files=pending_files, validity=getValidity())

@app.route('/login', methods=['GET', 'POST'])
def login():
    if request.method == 'POST':
        # Handle the login logic here
        username = request.form['username']
        password = request.form['password']
        
        db = admin.load()
        print(db)
        # Verify username and password here
        # For example, check against user data in the database
        for entry in db:
            print("Entry --- > ", entry)
            if username == entry["username"] and password == entry["password"]:
                session['logged_in'] = True
                session['name'] = entry["username"]
                if(entry["user_type"] == "admin"):
                    return redirect("/")  # Redirect to another page, e.g., home
                if(entry["user_type"] == "view"):
                    return redirect("/view")
        return "Login Failed"
    # GET request - show the login form
    return render_template('login.html')

@app.route('/view', methods=['GET', 'POST'])
def viewOnly():
    if not session.get('logged_in'):
        return redirect("/login")
    get_tx_req()
    return render_template('view.html',title="FileStorage",subtitle = "A Decentralized Network for File Storage/Sharing",node_address = ADDR,request_tx = request_tx)

@app.route('/approve', methods=['GET', 'POST'])
def approve():
    link = "{0}/mine".format(ADDR)
    resp = requests.get(link)
    return redirect("/")

@app.route('/delete', methods=['GET', 'POST'])
def delete():
    link = "{0}/delete".format(ADDR)
    resp = requests.get(link)
    return redirect("/")

@app.route('/account', methods=['GET','POST'])
def create_account():
    if request.method == 'POST':
        # Handle the login logic here
        username = request.form['username']
        password = request.form['password']
        user_type = request.form['user_type']

        admin.addAccount(username,password,user_type)
        return redirect("/")
    return render_template('account.html')

@app.route('/change_password', methods=['GET', 'POST'])
def change_password():
    if request.method == 'POST':
        # Handle the login logic here
        if not session.get('name'):
            return redirect("/login")
        print(session.get('name'))
        name = session.get('name')
        password = request.form['new_password']
        admin.changePass(name,password)
        return redirect("/")
    return render_template('change_password.html')


@app.route("/submit", methods=["POST"])
# When new transaction is created it is processed and added to transaction
def submit():
    start = timer()
    user = session['name']#request.form["user"]
    description = request.form["description"]
    up_file = request.files["v_file"]
    
    #save the uploaded file in destination
    up_file.save(os.path.join("app/Uploads/",secure_filename(up_file.filename)))
    #add the file to the list to create a download link
    files[up_file.filename] = os.path.join(app.root_path, "Uploads", up_file.filename)
    #determines the size of the file uploaded in bytes 
    file_states = os.stat(files[up_file.filename]).st_size 
    #create a transaction object
    up_file.stream.seek(0) #start reading at start of file
    #print("DATA========", up_file.stream.read())
    post_object = {
        "user": user, #user name
        "description": description, #user name
        "v_file" : up_file.filename, #filename
        "file_data" : up_file.stream.read().decode(),#str(up_file.stream.read()), #file data
        "file_size" : file_states   #file size
    }
    print(post_object)
    # Submit a new transaction
    address = "{0}/new_transaction".format(ADDR)
    requests.post(address, json=post_object)
    end = timer()
    print(end - start)
    return redirect("/")

@app.route("/submit/<string:variable>",methods = ["GET"])
def download_file(variable):
    p = files[variable]
    return send_file(p,as_attachment=True)

app.run(host="0.0.0.0", port=8000)
