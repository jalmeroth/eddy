function doXHR(url, callback, method, data) {
    console.debug("doXHR.");
    url = url || REDIRECT_URI;
    console.log(method + " URL: " + url + " Data:");
    console.log(data);
    method = method || "GET";
    data = data || null;
    var xhr = new XMLHttpRequest();
    xhr.open(method, url);
    xhr.onreadystatechange = callback;
    xhr.send(data);
}

function handleResponse(e) {
    if(this.readyState == this.DONE) {
        if(this.status === 200) {
            console.log(this.response);
        } else if(this.status === 204) {
            console.log("204: No Content");
        } else {
            console.log("Something went wrong.");
        }
    }
}

function buttonClicked(ev) {
    var target = ev.target;
    if(target.nodeName.toLowerCase() === "button") {
        doXHR('/color/set', handleResponse, 'POST', target.className);
    }
}

window.addEventListener("load", function(event) {
    buttons = document.getElementsByTagName('button');
    for (var i = 0; i < buttons.length; i++) {
        buttons[i].addEventListener('click', buttonClicked, false);
    }
});
