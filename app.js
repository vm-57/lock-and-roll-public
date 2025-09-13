var createError = require("http-errors");
var express = require("express");
var path = require("path");
var cookieParser = require("cookie-parser");
var logger = require("morgan");
const bodyParser = require("body-parser");
const { MessagingResponse } = require("twilio").twiml;
const twilio = require("twilio");
const axios = require("axios");
var indexRouter = require("./routes/index");
var usersRouter = require("./routes/users");
var fs = require("fs");
require('log-timestamp');

var app = express();

// view engine setup
app.set("views", path.join(__dirname, "views"));
app.set("view engine", "ejs");

app.use(logger("dev"));
app.use(express.json());
app.use(express.urlencoded({ extended: false }));
app.use(cookieParser());
app.use(express.static(path.join(__dirname, "public")));

app.use("/", indexRouter);
app.use("/users", usersRouter);

const twilioSid = process.env.TWILIO_ACCOUNT_SID;
const twilioAuthToken = process.env.TWILIO_AUTH_TOKEN;
const client = twilio(twilioSid, twilioAuthToken);
const pool = require("mysql2/promise").createPool({
  host: process.env.SQL_HOST,
  user: process.env.SQL_USERNAME,
  password: process.env.SQL_PASSWORD,
  database: process.env.SQL_DATABASE,
  port: 3306,
  ssl: {
    ca: fs.readFileSync("C:/home/site/wwwroot/DigiCertGlobalRootCA.crt.pem"),
  },
});

async function readTable() {
  const results = await pool.query("SELECT * FROM locks");
  results.forEach((row) => {
    console.log(row);
  });
}

async function lockExists(phoneNumber) {
  const exists = `SELECT * FROM locks WHERE phone_number = ${phoneNumber}`;
  console.log(`lockExists query: ${exists} \n`);
  const results = await pool.query(exists);
  console.log(`Results Length: ${results[0].length}`);
  if (results[0].length === 0) {
    console.log("Does not exist.\n");
    return false;
  }
  console.log("Does exist.\n");
  return true;
}

async function tooManyLocks(phoneNumber) {
  const exists = `SELECT * FROM locks WHERE phone_number = ${phoneNumber}`;
  console.log(`tooManyLocks query: ${exists} \n`);
  const results = await pool.query(exists);
  const returnedResults = JSON.parse(JSON.stringify(results[0]));
  if (returnedResults[0]) {
    if (returnedResults[0].num_locks >= 4) {
      console.log("Too many locks.\n");
      return true;
    }
  }
  console.log("Room for more locks.\n");
  return false;
}

async function getAllLockIDs(phoneNumber) {
  const exists = `SELECT * FROM locks WHERE phone_number = ${phoneNumber}`;
  console.log(`Exists query: ${exists} \n`);
  const results = await pool.query(exists);
  const returnedResults = JSON.parse(JSON.stringify(results[0]));
  console.log("Returned Results: ", returnedResults);
  if (returnedResults[0].length !== 0) {
    var LockIDs = [
      returnedResults[0].LockID1,
      returnedResults[0].LockID2 === 0 ? -1 : returnedResults[0].LockID2,
      returnedResults[0].LockID3 === 0 ? -1 : returnedResults[0].LockID3,
      returnedResults[0].LockID4 === 0 ? -1 : returnedResults[0].LockID4,
    ];
    return LockIDs;
  } else {
    return [-1, -1, -1, -1];
  }
}

async function getAllRecipients(lockID) {
  const exists = `SELECT phone_number FROM locks WHERE ${lockID} IN (LockID1, LockID2, LockID3, LockID4)`;
  console.log(`Exists query: ${exists} \n`);
  const results = await pool.query(exists);
  const returnedResults = JSON.parse(JSON.stringify(results[0]));
  var recipients = [];
  console.log("Returned Results: ", returnedResults);
  if (returnedResults.length !== 0) {
    returnedResults.forEach((user) => {
      console.log("User:", user);
      console.log("UserNumber:", user.phone_number);
      recipients.push(user.phone_number);
      console.log(`Recipients: ${recipients}`);
    });
    return recipients;
  } else {
    return [-1];
  }
}

async function insertLock(phoneNumber, lockID) {
  const exists = `SELECT * FROM locks WHERE phone_number = ${phoneNumber}`;
  console.log(`insertLock query: ${exists} \n`);
  const results = await pool.query(exists);
  const returnedResults = JSON.parse(JSON.stringify(results[0]));
  if (returnedResults.length === 0) {
    console.log("Adding a new row\n");
    // add a new row
    const insertQuery = `INSERT INTO locks VALUES (${phoneNumber}, ${lockID}, 0, 0, 0, 1, 2)`;
    console.log(`Insertion query: ${insertQuery}\n`);
    await pool.query(insertQuery);
    return true;
  } else {
    if (returnedResults[0].num_locks < 4) {
      console.log(`results: ${returnedResults[0].num_locks}\n`);
      const updateQuery1 = `UPDATE locks SET LockID${returnedResults[0].last_modified_lock} = ${lockID} WHERE phone_number = ${phoneNumber}`;
      console.log(`Update query 1: ${updateQuery1}\n`);
      const updateQuery2 = `UPDATE locks SET num_locks = ${
        returnedResults[0].num_locks + 1
      } WHERE phone_number = ${phoneNumber}`;
      console.log(`Update query 2: ${updateQuery2}\n`);
      const updateQuery3 = `UPDATE locks SET last_modified_lock = ${
        returnedResults[0].last_modified_lock + 1
      } WHERE phone_number = ${phoneNumber}`;
      console.log(`Update query 3: ${updateQuery3}\n`);
      await pool.query(updateQuery1);
      await pool.query(updateQuery2);
      await pool.query(updateQuery3);
      return true;
    } else {
      console.log("Too many locks!\n");
      return false;
    }
  }
}

async function deleteLock(phoneNumber, lockID) {
  const exists = `SELECT * FROM locks WHERE phone_number = ${phoneNumber}`;
  console.log(`Exists query: ${exists} \n`);
  const results = await pool.query(exists);
  const returnedResults = JSON.parse(JSON.stringify(results[0]));
  console.log("Returned Results: ", returnedResults);
  const LockIDs = await getAllLockIDs(phoneNumber);
  console.log(`LockIDs: ${LockIDs} \n`);
  console.log("Deleting a lock\n");
  associatedIndex = LockIDs.indexOf(lockID) + 1;
  console.log(`Associated Index: ${associatedIndex}\n`);
  const updateQuery1 = `UPDATE locks SET LockID${associatedIndex} = ${-1} WHERE phone_number = ${phoneNumber}`;
  console.log(`Update query 1: ${updateQuery1}\n`);
  const updateQuery2 = `UPDATE locks SET num_locks = ${
    returnedResults[0].num_locks - 1
  } WHERE phone_number = ${phoneNumber}`;
  console.log(`Update query 2: ${updateQuery2}\n`);
  const updateQuery3 = `UPDATE locks SET last_modified_lock = ${associatedIndex} WHERE phone_number = ${phoneNumber}`;
  console.log(`Update query 3: ${updateQuery3}\n`);
  await pool.query(updateQuery1);
  await pool.query(updateQuery2);
  await pool.query(updateQuery3);
}

// Debug log of all API requests
app.use((req, res, next) => {
  console.log(`Received ${req.method} request to ${req.url}`);
  console.log("Headers:", req.headers);
  console.log("Body:", req.body);

  next();
});

app.use(express.json());
app.use(bodyParser.urlencoded({ extended: false }));

// Twilio -> Hologram (SMSs sent from user to phone)
app.post("/inbound", async (req, res) => {
  console.log(`${req}\n`);
  const twiml = new MessagingResponse();

  // Log input SMS
  const message = req.body.Body.toUpperCase();
  const from = req.body.From;
  const sanitizedFrom = from.replace("+", "");

  if (message.startsWith("ID:")) {
    console.log("Registering a lock.\n");
    const too_many = await tooManyLocks(sanitizedFrom);
    console.log("too_many: ", too_many);
    if (too_many === true) {
      console.log("Case 2: Too many locks\n");
      client.messages.create({
        body: `[Lock and Roll] Would you like to replace a lock to make room for the new one? If so, reply with the associated Lock ID: ${await getAllLockIDs(
          sanitizedFrom
        )}. Format your message as 'DELETE:[ID]', without any spaces. Reply STOP to cancel.`,
        from: "+14153197830",
        to: from,
      });
    } else {
      console.log("Case 2.1: Not too many locks\n");
      try {
        await insertLock(sanitizedFrom, message.replace("ID:", ""));
        client.messages.create({
          body: `[Lock and Roll] New Lock ID inserted. Reply STOP to cancel.`,
          from: "+14153197830",
          to: from,
        });
      } catch {
        console.log("An error has occured when inserting the lock");
        client.messages.create({
          body: `[Lock and Roll] Unable to insert new Lock ID. Reply STOP to cancel.`,
          from: "+14153197830",
          to: from,
        });
      }
    }
  } else if (message.startsWith("DELETE:")) {
    console.log("Deleting a lock.\n");
    try {
      const lockIDs = await getAllLockIDs(sanitizedFrom);
      var existsFlag = false;
      for (var i = 0; i < lockIDs.length; i++) {
        if (
          lockIDs[i] !== -1 &&
          lockIDs[i] === parseInt(message.replace("DELETE:", ""))
        ) {
          console.log("existsFlag true");
          existsFlag = true;
        }
        console.log(lockIDs[i]);
      }
      if (existsFlag) {
        console.log("Lock found, now deleting\n");
        try {
          await deleteLock(
            sanitizedFrom,
            parseInt(message.replace("DELETE:", ""))
          );
          client.messages.create({
            body: `[Lock and Roll] Lock deleted. Reply STOP to cancel.`,
            from: "+14153197830",
            to: from,
          });
        } catch {
          console.log("An error has occured when deleting the lock");
          client.messages.create({
            body: `[Lock and Roll] Due to an error, the lock was unable to be deleted. Reply STOP to cancel.`,
            from: "+14153197830",
            to: from,
          });
        }
      } else {
        console.log("Lock not found. No need to delete.\n");
        client.messages.create({
          body: `[Lock and Roll] Invalid Lock ID passed in. Reply STOP to cancel.`,
          from: "+14153197830",
          to: from,
        });
      }
    } catch {
      console.log("An error has occured when retrieving all locks");
    }
  } else if (message === "LIST") {
    const allLockIDs = await getAllLockIDs(sanitizedFrom);
    client.messages.create({
      body: `[Lock and Roll] All Lock IDs: ${allLockIDs.filter(
        (element) => element !== -1
      )}. Reply STOP to cancel.`,
      from: "+14153197830",
      to: from,
    });
  } else {
    const exists = await lockExists(sanitizedFrom);
    console.log("exists: ", exists);
    if (!exists) {
      console.log("Case 1: Lock doesn't exist for user\n");
      client.messages.create({
        body: "[Lock and Roll] Enter your Lock and Roll Board ID. Format your message as 'ID:[ID]', without any spaces. Reply STOP to cancel.",
        from: "+14153197830",
        to: from,
      });
    } else {
      console.log("Case 1.1: Lock exists for user\n");
      console.log("\n----------\nNew Inbound SMS (Twilio -> Hologram)");
      console.log(`SMS received from ${from}: "${message}"`);

      // Twilio will retry the POST if a response is not sent
      twiml.message("");
      res.type("text/xml").send(twiml.toString());

      const lockIDs = await getAllLockIDs(sanitizedFrom);
      console.log("lockIDs: ", lockIDs);
      for (var i = 0; i < lockIDs.length; i++) {
        // if lockID == -1, not valid
        console.log(`Lock ID: ${lockIDs[i]}\n`);
        if (lockIDs[i] !== -1) {
          // Send SMS data to Hologram
          const payload = {
            deviceid: lockIDs[i],
            fromnumber: process.env.TWILIO_NUMBER,
            apikey: process.env.HOLOGRAM_API_KEY,
            body: from + "," + message,
          };
          try {
            await axios.post(
              "https://dashboard.hologram.io/api/1/sms/incoming",
              payload
            );
          } catch {
            console.log(`Unable to send message to LockID: ${lockIDs[i]}\n`);
          }
        }
      }
    }
  }
});

// Hologram -> Twilio (Data sent from board to user)
app.post("/outbound", async (req, res) => {
  console.log(`${req}\n`);
  console.log("\n----------\nNew Outbound SMS (Board -> Twilio)");

  const message = req.body.message;
  const recipient = req.body.recipient;

  console.log(`Message: ${message}`);

  client.messages
    .create({
      body: message,
      from: "+14153197830",
      to: recipient,
    })
    .then((message) =>
      console.log(
        `Sent message ${message} to ${recipient} with sid "${message.sid}"`
      )
    );

  res.send(`Message "${message}" for ${recipient} received!`);
  console.log("----------");
});

// Hologram -> Twilio (Data sent from board to user for ALARM)
app.post("/alarm", async (req, res) => {
  console.log(`${req}\n`);
  console.log("\n----------\nNew Outbound SMS [ALARM] (Board -> Twilio)");

  const location = req.body.location;
  const deviceID = req.body.deviceid;
  var recipients;
  try {
    recipients = await getAllRecipients(deviceID);
  } catch {
    console.log("No recipients, so the board must not have a registered user");
    return;
  }

  console.log("Recipients:", recipients);
  for (var i = 0; i < recipients.length; i++) {
    console.log("Recipient:", recipients[i]);
    client.messages.create({
      body: `[Lock and Roll] An alarm has been triggered. Your lock is currently at ${location}. Reply STOP to cancel.`,
      from: "+14153197830",
      to: recipients[i],
    });
  }
  console.log("----------");
});

// catch 404 and forward to error handler
app.use(function (req, res, next) {
  next(createError(404));
});

// error handler
app.use(function (err, req, res, next) {
  // set locals, only providing error in development
  res.locals.message = err.message;
  res.locals.error = req.app.get("env") === "development" ? err : {};

  // render the error page
  res.status(err.status || 500);
  res.render("error");
});

module.exports = app;
