use {
    crate::GlobalData,
    futures::{future::FutureExt, channel::mpsc},
    hyper::{header, service, Body, Method, Request, Response, StatusCode},
    indoc::indoc,
    r2d2_sqlite::SqliteConnectionManager,
    rusqlite::{params, OptionalExtension},
    std::{collections::HashMap, net::SocketAddr, sync::{Arc, Mutex}},
    tera::Tera,
    tokio::spawn,
};

pub async fn try_serv_upgrade(gda: &Arc<GlobalData>, req_parts: &http::request::Parts, req_body: &mut Body, user: &str) -> Option<Response<Body>> {
    // TODO: add websocket support using tokio-tungstenite
    // see also: http://zderadicka.eu/hyper-websocket/
    None
}
