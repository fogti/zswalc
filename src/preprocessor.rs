use std::borrow::Cow;

struct NPResult<'a> {
    parsed: String,
    rest: &'a str,
}

/// This function handles the encoding of messages
/// to prevent XSS attacks.
pub fn preprocess_msg(mut input: &str) -> Cow<'_, str> {
    // TODO: only copy string if necessary
    let mut ret = String::with_capacity(input.len());
    let mut buffer = [0; 4];
    loop {
        let orig_input = input;
        let x = {
            let mut it = input.chars();
            let x = if let Some(x) = it.next() {
                x
            } else {
                break;
            };
            input = it.as_str();
            x
        };

        match x {
            '[' => {
                let r = parse_element(orig_input);
                ret += &r.parsed;
                input = r.rest;
            }
            ']' => {}
            _ => ret += &htmlescape::encode_minimal(x.encode_utf8(&mut buffer)),
        }
    }
    Cow::Owned(ret)
}

/// 1. part while f(x) == true, then 2. part
pub fn str_split_at_while(x: &str, mut f: impl FnMut(char) -> bool) -> (&str, &str) {
    x.split_at(
        x.chars()
            .take_while(|i| f(*i))
            .map(|i| i.len_utf8())
            .sum::<usize>(),
    )
}

fn parse_element(input: &str) -> NPResult<'_> {
    let mut it = input.chars();
    if !input.is_empty() && it.next() == Some('[') {
        let (fi, se) = str_split_at_while(it.as_str(), |x| x != ']');
        let mut is_valid = !se.is_empty();
        let se = if is_valid { &se[']'.len_utf8()..] } else { se };
        NPResult {
            parsed: if is_valid {
                let (cmd, args) = str_split_at_while(fi, |x| !x.is_whitespace());
                // filter out everything we don't want
                // currently we don't support inner nesting
                is_valid = is_valid
                    && match cmd {
                        "a" | "b" | "i" | "/a" | "/b" | "/i" => !args.contains('['),
                        _ => false,
                    };
                if is_valid {
                    let mut acc = String::with_capacity(2 + fi.len());
                    acc += "<";
                    acc += fi;
                    acc += ">";
                    Some(acc)
                } else {
                    None
                }
            } else {
                None
            }
            .unwrap_or_else(|| htmlescape::encode_minimal(fi)),
            rest: se,
        }
    } else {
        NPResult {
            parsed: String::new(),
            rest: input,
        }
    }
}
