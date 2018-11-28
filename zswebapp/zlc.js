"use strict";

var lctag = undefined;
var do_notif = false;
var got_msg = false;
document.show_chat = 'cur';

if("Notification" in window) {
  if(Notification.permission === 'granted')
    do_notif = true;
  else if(Notification.permission !== 'denied') {
    Notification.requestPermission(function(permission) {
      if(permission === "granted") do_notif = true;
    });
  }

  function handle_notif() {
    if(!do_notif) return;
    if(got_msg) {
      let notif = new Notification("ZSChat Nachricht");
    }
    got_msg = false;
    setTimeout(handle_notif, 60000);
  }
  setTimeout(handle_notif, 60000);
}

function loadchat() {
  let xhr = new XMLHttpRequest();
  xhr.onload = function(event) {
    if(xhr.status == 304) {
      setTimeout(loadchat, 60000);
      return;
    }
    setTimeout(loadchat, 30000);
    if(xhr.status < 200 || xhr.status >= 300) {
      console.warn(xhr.statusText, xhr.responseText);
      return;
    }
    document.getElementById('chat').innerHTML = xhr.responseText;
    if(lctag) got_msg = true;
    lctag = xhr.getResponseHeader('X-ChatTag');
  };
  let ctgp = '';
  if(lctag) ctgp = '&t='+lctag;
  xhr.open('GET', '?g='+document.show_chat+ctgp);
  xhr.send();
}
