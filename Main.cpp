#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <filesystem>
using namespace std;

// Read a file and return the contents as a string.
template <typename F>
string readFromFile(const F& file)
{
    ifstream filein { file };
    string nextLine;
    ostringstream stringout {};
    while (filein && getline(filein, nextLine))
    {
        stringout << nextLine << endl;
    }
    return stringout.str();
}

struct Team
{
public:
    int id;
    string name;
    string membersJSON;
};

struct Question
{
public:
    int id;
    string name;
    string templateJSON;
};

void replaceAll(string& source, const string& original, const string& replacement)
{
    // Find each position in the source where the original occurs and replace it.
    // Brute force implementation since there won't be that many replacements to make.
    for (auto pos { source.find(original) }; pos != string::npos; pos = source.find(original, pos))
    {
        source.replace(pos, original.length(), replacement);
    }
}

// Populates the template with team information
string populateTemplate(const Question& question, const Team& team)
{
    string templateString { question.templateJSON };
    replaceAll(templateString, "@QID@", "QID_Team" + to_string(team.id) + "." + to_string(question.id));
    replaceAll(templateString, "@QTag@", question.name + "/" + team.name);
    replaceAll(templateString, "@TeamID@", to_string(team.id));
    replaceAll(templateString, "@TeamName@", team.name);
    replaceAll(templateString, "@TeamMembers@", team.membersJSON);
    return templateString;
}

// Returns a string containing a copy of the template question populated for each team.
string populateForAllTeams(const Question& question, const vector<Team>& teams)
{
    ostringstream stringout;

    for (const Team& team : teams)
    {
        stringout << populateTemplate(question, team);
        if (&team != &teams.back())
        {
            stringout << ',' << endl; // Only print comma if not the last team
        }
    }

    return stringout.str();
}

string buildTeamChoices(const vector<Team>& teams)
{
    ostringstream stringout;

    // Print team choices
    stringout << "\"Choices\" : { " << endl;
    for (const Team& team : teams)
    {
        stringout << '\"' << team.id << "\": {" << endl;
        stringout << "\"Display\": \"" << team.name << "\"" << endl;

        stringout << "}";
        if (&team != &teams.back())
        {
            stringout << ','; // Only print comma if not the last team
        }
        stringout << endl;
    }
    stringout << "}," << endl;

    // Specify the choice order
    stringout << "\"ChoiceOrder\" : [ " << endl;
    for (size_t i { 1 }; i <= teams.size(); i++)
    {
        stringout << i;
        if (i != teams.size())
        {
            stringout << ','; // Only print comma if not the last team
        }
        stringout << endl;
    }
    stringout << "]," << endl;

    // Specify which choice IDs would be used if more teams were to be added
    stringout << "\"NextChoiceId\": " << teams.size() + 1;

    return stringout.str();
}

// Wraps C-style args as a vector of strings.
// Skips argument 0 (the executable name).
vector<string> wrapArgs(int argc, char** argv)
{
    vector<string> args {};

    for (int i { 1 }; i < argc; i++)
    {
        args.emplace_back(argv[i]);
    }

    return args;
}

// Gets the base filename, without directory path or file extension.
string getBaseFileName(string filePath)
{
    auto startPos { filePath.find_last_of('/') };
    if (startPos == string::npos)
    {
        // If no forward slash is found, look for a backslash.
        startPos = filePath.find_last_of('\\');
    }
    else
    {
        // If a forward slash was found, still check if there was a closer backslash.
        auto altStartPos { filePath.find_last_of('\\') };

        if (altStartPos != string::npos)
        {
            startPos = max(startPos, altStartPos);
        }
    }
    return filePath.substr(startPos + 1, filePath.find_last_of('.') - startPos - 1);
}

int main(int argc, char* argv[])
{
    vector<string> args { wrapArgs(argc, argv) };

    // Last argument is the output file (not a team specification file).
    string outFile = args.back();
    args.pop_back();

    // Populate list of teams from command-line arguments.
    vector<Team> teams {};
    int teamID { 1 };
    for (string filename : args)
    {
        // Team name is the filename without the .json.
        string teamName { getBaseFileName(filename) };
        teams.push_back({ teamID, teamName, readFromFile(filename) });
        teamID++;
    }

    // Read root template
    string rootTemplate { readFromFile("TeamSkillsTemplate.json") };

    // Define team choices for the "What team are you on" question.
    replaceAll(rootTemplate, "@TeamChoices@", buildTeamChoices(teams));

    // Read template for a question reference in the question list.
    string baseQuestionRefTemplate { readFromFile("QuestionRef.json") };

    int id { 1 };

    // Iterate templates in "Questions" directory,.
    for (const auto& file : std::filesystem::directory_iterator("./Questions"))
    {
        // Template name = file name without .json.
        string templateName { getBaseFileName(file.path().string()) };

        // Read each template and populate it for all teams, then replace the placeholder string in the root template.
        Question qTemplate { id, templateName, readFromFile(file.path()) };
        Question questionRefTemplate { id, templateName, baseQuestionRefTemplate };
        replaceAll(rootTemplate, "@" + templateName + "@", populateForAllTeams(qTemplate, teams));
        replaceAll(rootTemplate, "@" + templateName + "Refs@", populateForAllTeams(questionRefTemplate, teams));

        id++;
    }

    // Write out the final file to the specified output file.
    ofstream fileout { outFile };
    fileout << rootTemplate;

    return 0;
}
